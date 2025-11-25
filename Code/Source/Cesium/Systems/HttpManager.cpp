#include "Cesium/Systems/HttpManager.h"
#include <AzCore/PlatformDef.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobFunction.h>
#include <CesiumUtility/Uri.h>
#include <CesiumAsync/Promise.h>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <span>

// Use Cesium Native's CurlAssetAccessor for cross-platform HTTP support
// This avoids needing direct access to curl headers
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetResponse.h>

namespace Cesium
{
    // Internal HTTP client implementation using Cesium Native's CurlAssetAccessor
    struct HttpManager::HttpClientImpl
    {
        std::shared_ptr<CesiumCurl::CurlAssetAccessor> m_curlAccessor;
        
        HttpClientImpl()
        {
            CesiumCurl::CurlAssetAccessorOptions options;
            options.userAgent = "CesiumForO3DE/1.0";
            m_curlAccessor = std::make_shared<CesiumCurl::CurlAssetAccessor>(options);
        }
        
        ~HttpClientImpl()
        {
            // CurlAssetAccessor will clean up automatically
        }
        
        std::shared_ptr<HttpResponse> MakeRequest(
            const CesiumAsync::AsyncSystem& asyncSystem,
            const std::string& url,
            HttpMethod method,
            const CesiumAsync::HttpHeaders& headers,
            const std::string& body)
        {
            auto response = std::make_shared<HttpResponse>();
            
            // Convert headers to CurlAssetAccessor format
            std::vector<CesiumAsync::IAssetAccessor::THeader> requestHeaders;
            for (const auto& header : headers)
            {
                requestHeaders.push_back({header.first, header.second});
            }
            
            // Convert HTTP method to string
            std::string methodStr = "GET";
            switch (method)
            {
            case HttpMethod::HTTP_GET:
                methodStr = "GET";
                break;
            case HttpMethod::HTTP_POST:
                methodStr = "POST";
                break;
            case HttpMethod::HTTP_PUT:
                methodStr = "PUT";
                break;
            case HttpMethod::HTTP_DELETE:
                methodStr = "DELETE";
                break;
            case HttpMethod::HTTP_HEAD:
                methodStr = "HEAD";
                break;
            case HttpMethod::HTTP_PATCH:
                methodStr = "PATCH";
                break;
            }
            
            // Convert body to span of bytes
            std::span<const std::byte> bodySpan;
            std::vector<std::byte> bodyBytes;
            if (!body.empty())
            {
                bodyBytes.resize(body.size());
                std::memcpy(bodyBytes.data(), body.data(), body.size());
                bodySpan = std::span<const std::byte>(bodyBytes.data(), bodyBytes.size());
            }
            
            // Make the request synchronously
            try
            {
                CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> requestFuture = 
                    (methodStr == "GET") 
                        ? m_curlAccessor->get(asyncSystem, url, requestHeaders)
                        : m_curlAccessor->request(asyncSystem, methodStr, url, requestHeaders, bodySpan);
                
                // Wait for the request to complete
                auto assetRequest = requestFuture.wait();
                
                if (assetRequest && assetRequest->response())
                {
                    const CesiumAsync::IAssetResponse* assetResponse = assetRequest->response();
                    response->m_statusCode = assetResponse->statusCode();
                    response->m_contentType = assetResponse->contentType();
                    response->m_headers = assetResponse->headers();
                    
                    // Copy response data
                    auto dataSpan = assetResponse->data();
                    response->m_body.resize(dataSpan.size());
                    if (dataSpan.size() > 0)
                    {
                        memcpy(response->m_body.data(), dataSpan.data(), dataSpan.size());
                    }
                }
                else
                {
                    response->m_statusCode = 0;
                }
            }
            catch (const std::exception& e)
            {
                // Log error if needed, but don't throw
                response->m_statusCode = 0;
            }
            catch (...)
            {
                // Catch any other exceptions
                response->m_statusCode = 0;
            }
            
            return response;
        }
    };
    
    struct HttpManager::RequestHandler
    {
        RequestHandler(
            HttpClientImpl* httpClient,
            HttpRequestParameter&& httpRequestParameter,
            const CesiumAsync::Promise<HttpResult>& promise,
            const CesiumAsync::AsyncSystem& asyncSystem)
            : m_httpClient{ httpClient }
            , m_httpRequestParameter{ std::move(httpRequestParameter) }
            , m_promise{ promise }
            , m_asyncSystem{ asyncSystem }
        {
        }

        void operator()()
        {
            // Convert AZStd::string to std::string safely
            std::string urlStr(m_httpRequestParameter.m_url.c_str(), m_httpRequestParameter.m_url.length());
            std::string bodyStr(m_httpRequestParameter.m_body.c_str(), m_httpRequestParameter.m_body.length());
            
            auto httpResponse = m_httpClient->MakeRequest(
                m_asyncSystem,
                urlStr,
                m_httpRequestParameter.m_method,
                m_httpRequestParameter.m_headers,
                bodyStr);
            
            auto httpRequest = std::make_shared<HttpRequest>();
            httpRequest->m_url = urlStr;
            httpRequest->m_method = m_httpRequestParameter.m_method;
            httpRequest->m_headers = m_httpRequestParameter.m_headers;
            httpRequest->m_body = bodyStr;
            
            m_promise.resolve({ httpRequest, httpResponse });
        }

        HttpClientImpl* m_httpClient;
        HttpRequestParameter m_httpRequestParameter;
        CesiumAsync::Promise<HttpResult> m_promise;
        CesiumAsync::AsyncSystem m_asyncSystem;
    };

    struct HttpManager::GenericIORequestHandler
    {
        GenericIORequestHandler(
            HttpClientImpl* httpClient,
            const IORequestParameter& request,
            const CesiumAsync::Promise<IOContent>& promise,
            const CesiumAsync::AsyncSystem& asyncSystem)
            : m_httpClient{ httpClient }
            , m_request{ request }
            , m_promise{ promise }
            , m_asyncSystem{ asyncSystem }
        {
        }

        GenericIORequestHandler(
            HttpClientImpl* httpClient,
            IORequestParameter&& request,
            const CesiumAsync::Promise<IOContent>& promise,
            const CesiumAsync::AsyncSystem& asyncSystem)
            : m_httpClient{ httpClient }
            , m_request{ std::move(request) }
            , m_promise{ promise }
            , m_asyncSystem{ asyncSystem }
        {
        }

        void operator()()
        {
            std::string absoluteUrl = CesiumUtility::Uri::resolve(m_request.m_parentPath.c_str(), m_request.m_path.c_str());

            auto httpResponse = m_httpClient->MakeRequest(
                m_asyncSystem,
                absoluteUrl,
                HttpMethod::HTTP_GET,
                CesiumAsync::HttpHeaders{},
                std::string{});
            
            if (httpResponse)
            {
                m_promise.resolve(httpResponse->m_body);
            }
            else
            {
                m_promise.resolve(IOContent{});
            }
        }

        HttpClientImpl* m_httpClient;
        IORequestParameter m_request;
        CesiumAsync::Promise<IOContent> m_promise;
        CesiumAsync::AsyncSystem m_asyncSystem;
    };

    HttpManager::HttpManager()
    {
        AZ::JobManagerDesc jobDesc;
        for (size_t i = 0; i < AZStd::thread::hardware_concurrency(); ++i)
        {
            jobDesc.m_workerThreads.push_back({ static_cast<int>(i) });
        }
        m_ioJobManager = AZStd::make_unique<AZ::JobManager>(jobDesc);
        m_ioJobContext = AZStd::make_unique<AZ::JobContext>(*m_ioJobManager);
        m_httpClient = AZStd::make_unique<HttpClientImpl>();
    }

    HttpManager::~HttpManager() noexcept
    {
        m_ioJobContext.reset();
        m_ioJobManager.reset();
        m_httpClient.reset();
    }

    CesiumAsync::Future<HttpResult> HttpManager::AddRequest(
        const CesiumAsync::AsyncSystem& asyncSystem, HttpRequestParameter&& httpRequestParameter)
    {
        auto promise = asyncSystem.createPromise<HttpResult>();
        AZ::Job* job = aznew AZ::JobFunction<std::function<void()>>(
            RequestHandler{ m_httpClient.get(), std::move(httpRequestParameter), promise, asyncSystem }, true, m_ioJobContext.get());
        job->Start();

        return promise.getFuture();
    }

    AZStd::string HttpManager::GetParentPath(const AZStd::string& path)
    {
        auto lastSlashPos = path.rfind('/');
        if (lastSlashPos == AZStd::string::npos)
        {
            return path;
        }

        return path.substr(0, lastSlashPos + 1);
    }

    IOContent HttpManager::GetFileContent(const IORequestParameter& request)
    {
        std::string absoluteUrl = CesiumUtility::Uri::resolve(request.m_parentPath.c_str(), request.m_path.c_str());

        // Create a temporary AsyncSystem for synchronous requests
        CesiumAsync::AsyncSystem asyncSystem(nullptr);
        auto httpResponse = m_httpClient->MakeRequest(
            asyncSystem,
            absoluteUrl,
            HttpMethod::HTTP_GET,
            CesiumAsync::HttpHeaders{},
            std::string{});
        
        if (!httpResponse)
        {
            return {};
        }

        return httpResponse->m_body;
    }

    IOContent HttpManager::GetFileContent(IORequestParameter&& request)
    {
        return GetFileContent(request);
    }

    CesiumAsync::Future<IOContent> HttpManager::GetFileContentAsync(
        const CesiumAsync::AsyncSystem& asyncSystem, const IORequestParameter& request)
    {
        auto promise = asyncSystem.createPromise<IOContent>();
        AZ::Job* job = aznew AZ::JobFunction<std::function<void()>>(
            GenericIORequestHandler{ m_httpClient.get(), request, promise, asyncSystem }, true, m_ioJobContext.get());
        job->Start();

        return promise.getFuture();
    }

    CesiumAsync::Future<IOContent> HttpManager::GetFileContentAsync(
        const CesiumAsync::AsyncSystem& asyncSystem, IORequestParameter&& request)
    {
        auto promise = asyncSystem.createPromise<IOContent>();
        AZ::Job* job = aznew AZ::JobFunction<std::function<void()>>(
            GenericIORequestHandler{ m_httpClient.get(), std::move(request), promise, asyncSystem }, true, m_ioJobContext.get());
        job->Start();

        return promise.getFuture();
    }

    IOContent HttpManager::GetResponseBodyContent(const HttpResponse& response)
    {
        return response.m_body;
    }
} // namespace Cesium
