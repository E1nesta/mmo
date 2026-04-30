#pragma once

#include <string>

#include "runtime/observability/metrics.h"

namespace mmo::runtime::observability {

std::string render_prometheus_metrics(const MetricsSnapshot& snapshot);

}  // namespace mmo::runtime::observability
