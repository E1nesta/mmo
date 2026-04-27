#include "runtime/http/http_router.h"

#include <iostream>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main() {
    framework::http::HttpRouter router;
    router.RegisterPost("/api/v1/test", [](const framework::http::HttpRequest& request) {
        return framework::http::HttpResponse{200, "application/x-protobuf", request.body, {}};
    });

    const auto ok = router.Handle({"POST", "/api/v1/test", "hello", {}});
    if (!Expect(ok.status == 200 && ok.content_type == "application/x-protobuf" && ok.body == "hello",
                "expected registered POST route to dispatch")) {
        return 1;
    }

    const auto missing = router.Handle({"POST", "/api/v1/missing", "", {}});
    if (!Expect(missing.status == 404, "expected missing route to return 404")) {
        return 1;
    }

    const auto method = router.Handle({"GET", "/api/v1/test", "", {}});
    if (!Expect(method.status == 405, "expected invalid method to return 405")) {
        return 1;
    }

    return 0;
}
