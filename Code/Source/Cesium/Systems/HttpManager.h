#pragma once

#include "Cesium/Systems/GenericIOManager.h"
#include <AzCore/std/string/string.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/HttpHeaders.h>
#include <string>
#include <memory>

namespace AZ
{
    class JobManager;
    class JobContext;
    class Job;
} // namespace AZ

namespace Cesium
{
    enum class HttpMethod
    {
        HTTP_GET,
        HTTP_POST,
        HTTP_PUT,
        HTTP_DELETE,
        HTTP_HEAD,
        HTTP_PATCH
    };

    struct HttpRequestParameter final
    {
        HttpRequestParameter(AZStd::string&& url, HttpMethod method)
            : m_url{ std::move(url) }
            , m_method{ method }
        {
        }

        HttpRequestParameter(AZStd::string&& url, HttpMethod method, CesiumAsync::HttpHeaders&& headers)
            : m_url{ std::move(url) }
            , m_method{ method }
            , m_headers{ std::move(headers) }
        {
        }

        HttpRequestParameter(AZStd::string&& url, HttpMethod method, CesiumAsync::HttpHeaders&& headers, AZStd::string&& body)
            : m_url{ std::move(url) }
            , m_method{ method }
            , m_headers{ std::move(headers) }
            , m_body{ std::move(body) }
        {
        }

        AZStd::string m_url;
        HttpMethod m_method;
        CesiumAsync::HttpHeaders m_headers;
        AZStd::string m_body;
    };

    struct HttpResponse final
    {
        int m_statusCode = 0;
        CesiumAsync::HttpHeaders m_headers;
        IOContent m_body;
        std::string m_contentType;
    };

    struct HttpRequest final
    {
        std::string m_url;
        HttpMethod m_method;
        CesiumAsync::HttpHeaders m_headers;
        std::string m_body;
    };

    struct HttpResult final
    {
        std::shared_ptr<HttpRequest> m_request;
        std::shared_ptr<HttpResponse> m_response;
    };

    class HttpManager final : public GenericIOManager
    {
        struct RequestHandler;
        struct GenericIORequestHandler;

    public:
        HttpManager();

        ~HttpManager() noexcept;

        CesiumAsync::Future<HttpResult> AddRequest(
            const CesiumAsync::AsyncSystem& asyncSystem, HttpRequestParameter&& httpRequestParameter);

        AZStd::string GetParentPath(const AZStd::string& path) override;

        IOContent GetFileContent(const IORequestParameter& request) override;

        IOContent GetFileContent(IORequestParameter&& request) override;

        CesiumAsync::Future<IOContent> GetFileContentAsync(
            const CesiumAsync::AsyncSystem& asyncSystem, const IORequestParameter& request) override;

        CesiumAsync::Future<IOContent> GetFileContentAsync(
            const CesiumAsync::AsyncSystem& asyncSystem, IORequestParameter&& request) override;

        static IOContent GetResponseBodyContent(const HttpResponse& response);

    private:
        AZStd::unique_ptr<AZ::JobManager> m_ioJobManager;
        AZStd::unique_ptr<AZ::JobContext> m_ioJobContext;
        
        // Internal HTTP client implementation
        struct HttpClientImpl;
        std::unique_ptr<HttpClientImpl> m_httpClient;
    };
} // namespace Cesium
