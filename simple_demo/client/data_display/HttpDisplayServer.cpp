#include "DataDisplayFlow.hpp"

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <sstream>
#include <iostream>
#include <mutex>

using boost::asio::ip::tcp;
namespace http = boost::beast::http;

class ConnectionHandler {
private:
    boost::asio::io_context *svc_;
    tcp::acceptor *acceptor_;
    tcp::socket socket_;
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    double *valueStorage_;
    std::mutex *valueStorageMutex_;
    std::string path_;
public:
    ConnectionHandler(boost::asio::io_context *svc, tcp::acceptor *acceptor, double *valueStorage, std::mutex *valueStorageMutex, std::string const &path)
        :
        svc_(svc), acceptor_(acceptor)
        , socket_(*svc), buffer_(), req_()
        , valueStorage_(valueStorage), valueStorageMutex_(valueStorageMutex)
        , path_(path)
    {
        acceptor_->async_accept(socket_, [this](boost::system::error_code const &ec) {
            new ConnectionHandler(svc_, acceptor_, valueStorage_, valueStorageMutex_, path_);

            if (ec) {
                delete this;
                return;
            }

            http::async_read(socket_, buffer_, req_, [this](boost::system::error_code const &read_ec, std::size_t bytes_read) {
                if (read_ec) {
                    delete this;
                    return;
                }

                if (req_.target() == "/data") {
                    std::ostringstream oss;
                    {
                        std::lock_guard<std::mutex> _(*valueStorageMutex_);
                        oss << std::fixed << std::setprecision(6) << *valueStorage_;
                    }
                    
                    auto *res = new http::response<http::string_body> {http::status::ok, req_.version()};
                    res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res->set(http::field::content_type, "text/plain");
                    res->keep_alive(req_.keep_alive());
                    res->body() = oss.str();
                    res->prepare_payload();
                    http::async_write(socket_, *res, [this,res](boost::system::error_code const &write_ec, std::size_t bytes_written) {
                        delete res;
                        delete this;
                    });
                } else if (req_.target() == "/") {
                    boost::beast::error_code bodyReadEc;
                    http::file_body::value_type body;
                    body.open((path_+"/index.html").c_str(), boost::beast::file_mode::scan, bodyReadEc);
                    auto *res = new http::response<http::file_body> {
                        std::piecewise_construct
                        , std::make_tuple(std::move(body))
                        , std::make_tuple(http::status::ok, req_.version())
                    };
                    res->set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res->set(http::field::content_type, "text/html");
                    res->keep_alive(req_.keep_alive());
                    res->prepare_payload();
                    http::async_write(socket_, *res, [this,res](boost::system::error_code const &write_ec, std::size_t bytes_written) {
                        delete res;
                        delete this;
                    });
                }
            });

        });
    }
    ~ConnectionHandler() = default;
};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: http_display_server ROOT_PATH [PORT]\n";
        return 0;
    }
    int port = (argc>=3)?std::atoi(argv[2]):23456;
    boost::asio::io_context io_service;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));
    double valueStorage;
    std::mutex valueStorageMutex;

    new ConnectionHandler(&io_service, &acceptor, &valueStorage, &valueStorageMutex, argv[1]);

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        transport::CrossGuidComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::AllNetworkTransportComponents
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.setLogFilePrefix("http_display_server", true);

    R r(&env);
    auto dataPrinter = M::pureExporter<simple_demo::InputData>(
        [&valueStorage,&valueStorageMutex](simple_demo::InputData &&d) {
            std::lock_guard<std::mutex> _(valueStorageMutex);
            valueStorage = d.value();
        }
    );
    r.registerExporter("dataPrinter", dataPrinter);
    dataDisplayFlow<TheEnvironment>(
        r
        , r.sinkAsSinkoid(r.exporterAsSink(dataPrinter))
    );

    r.finalize();

    //we don't need the termination controller here because the main loop
    //is taken over by io_service.run()

    boost::asio::io_context::work work(io_service);
    io_service.run();
}
