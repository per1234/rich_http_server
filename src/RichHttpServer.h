#include <Arduino.h>
#include <PathVariableHandler.h>
#include <RequestHandlers.h>
#include <vector>

#if defined(PVH_USE_ASYNC_WEBSERVER)
#include <ESPAsyncWebServer.h>
#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#elif defined(ESP32)
#include <WebServer.h>
#else
#error "Unsupported platform.  Only works with ESP8266 and ESP32"
#endif

class HandlerBuilder;

class RichHttpServer : public TServerType {
public:
  RichHttpServer(int port) : TServerType(port) { }
  ~RichHttpServer();

  HandlerBuilder& buildHandler(const String& path, bool disableAuth = false);

  // Enables authentication using the provided username and password
  void requireAuthentication(const String& username, const String& password);

  // Disables authentication
  void disableAuthentication();

  // Returns true if authentication is currently enabled
  bool isAuthenticationEnabled() const;

  // Returns true if there's currently a client connected to the server.
#ifndef PVH_ASYNC_WEBSERVER
  bool isClientConnected() {
    return _currentClient && _currentClient.connected();
  }
#endif

  // Validates that client has provided auth for a particular request
#ifndef PVH_ASYNC_WEBSERVER
  bool validateAuthentication();
#else
  bool validateAuthentication(AsyncWebServerRequest* request) {
    return !isAuthenticationEnabled() || request->authenticate(username.c_str(), password.c_str());
  }
#endif

private:
  bool authEnabled;
  String username;
  String password;

  std::vector<HandlerBuilder*> handlerBuilders;
};

class HandlerBuilder {
public:
  HandlerBuilder(RichHttpServer& server, const String& path, const bool disableAuth = false);

  template <typename TMethod>
  HandlerBuilder& on(const TMethod verb, PathVariableHandler::TPathVariableHandlerFn fn) {
    server.addHandler(new PathVariableHandler(path.c_str(), verb, buildAuthedHandler(fn)));
    return *this;
  }

  template <typename TMethod, typename THandler>
  HandlerBuilder& on(const TMethod verb, PathVariableHandler::TPathVariableHandlerFn fn, THandler bodyFn) {
#ifndef PVH_ASYNC_WEBSERVER
    server.addHandler(
      new RichFunctionRequestHandler(
        buildAuthedHandler(fn),
        bodyFn,
        path,
        verb
      )
    );
#else
    PathVariableHandler* handler = new PathVariableHandler(
      path.c_str(),
      verb,
      fn,
      bodyFn
    );
    server.addHandler(handler);
#endif

    return *this;
  }

private:
  bool disableAuth;
  const String path;
  RichHttpServer& server;

  template <class RetType, class ...Us>
  inline std::function<RetType(Us... args)> buildAuthedHandler(std::function<RetType(Us... args)> fn) {
    return [fn, this](Us... args) {
      if (disableAuth || server.validateAuthentication()) {
        return fn(args...);
      }
    };
  };
};