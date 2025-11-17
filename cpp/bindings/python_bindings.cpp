#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <memory>
#include "flowgen/flow_record.hpp"
#include "flowgen/generator.hpp"
#include "flowgen/utils.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_flowgen_core, m) {
    m.doc() = "FlowGen C++ core library - high-performance network flow generation";

    // FlowRecord binding
    py::class_<flowgen::FlowRecord>(m, "FlowRecord")
        .def(py::init<>())
        .def(py::init<uint32_t, uint32_t, uint16_t, uint16_t,
                     uint8_t, uint64_t, uint32_t>(),
             py::arg("source_ip"),
             py::arg("destination_ip"),
             py::arg("source_port"),
             py::arg("destination_port"),
             py::arg("protocol"),
             py::arg("timestamp_ns"),
             py::arg("packet_length"))
        .def(py::init<const std::string&, const std::string&, uint16_t, uint16_t,
                     uint8_t, uint64_t, uint32_t>(),
             py::arg("source_ip_str"),
             py::arg("destination_ip_str"),
             py::arg("source_port"),
             py::arg("destination_port"),
             py::arg("protocol"),
             py::arg("timestamp_ns"),
             py::arg("packet_length"))
        .def_readwrite("source_ip", &flowgen::FlowRecord::source_ip)
        .def_readwrite("destination_ip", &flowgen::FlowRecord::destination_ip)
        .def_property_readonly("source_ip_str", &flowgen::FlowRecord::source_ip_str)
        .def_property_readonly("destination_ip_str", &flowgen::FlowRecord::destination_ip_str)
        .def_readwrite("source_port", &flowgen::FlowRecord::source_port)
        .def_readwrite("destination_port", &flowgen::FlowRecord::destination_port)
        .def_readwrite("protocol", &flowgen::FlowRecord::protocol)
        .def_readwrite("timestamp", &flowgen::FlowRecord::timestamp)
        .def_readwrite("packet_length", &flowgen::FlowRecord::packet_length)
        .def("to_csv", &flowgen::FlowRecord::to_csv)
        .def_static("csv_header", &flowgen::FlowRecord::csv_header)
        .def("__repr__", [](const flowgen::FlowRecord& f) {
            return "FlowRecord(" + f.source_ip_str() + ":" + std::to_string(f.source_port) +
                   " -> " + f.destination_ip_str() + ":" + std::to_string(f.destination_port) +
                   ", proto=" + std::to_string(f.protocol) +
                   ", len=" + std::to_string(f.packet_length) + "B" +
                   ", ts=" + std::to_string(f.timestamp) + ")";
        });

    // TrafficPattern binding
    py::class_<flowgen::GeneratorConfig::TrafficPattern>(m, "TrafficPattern")
        .def(py::init<>())
        .def_readwrite("type", &flowgen::GeneratorConfig::TrafficPattern::type)
        .def_readwrite("percentage", &flowgen::GeneratorConfig::TrafficPattern::percentage);

    // GeneratorConfig binding
    py::class_<flowgen::GeneratorConfig>(m, "GeneratorConfig")
        .def(py::init<>())
        .def_readwrite("bandwidth_gbps", &flowgen::GeneratorConfig::bandwidth_gbps)
        .def_readwrite("flows_per_second", &flowgen::GeneratorConfig::flows_per_second)
        .def_readwrite("max_flows", &flowgen::GeneratorConfig::max_flows)
        .def_readwrite("duration_seconds", &flowgen::GeneratorConfig::duration_seconds)
        .def_readwrite("start_timestamp_ns", &flowgen::GeneratorConfig::start_timestamp_ns)
        .def_readwrite("source_subnets", &flowgen::GeneratorConfig::source_subnets)
        .def_readwrite("destination_subnets", &flowgen::GeneratorConfig::destination_subnets)
        .def_readwrite("source_weights", &flowgen::GeneratorConfig::source_weights)
        .def_readwrite("min_packet_size", &flowgen::GeneratorConfig::min_packet_size)
        .def_readwrite("max_packet_size", &flowgen::GeneratorConfig::max_packet_size)
        .def_readwrite("average_packet_size", &flowgen::GeneratorConfig::average_packet_size)
        .def_readwrite("bidirectional_mode", &flowgen::GeneratorConfig::bidirectional_mode)
        .def_readwrite("bidirectional_probability", &flowgen::GeneratorConfig::bidirectional_probability)
        .def_readwrite("traffic_patterns", &flowgen::GeneratorConfig::traffic_patterns)
        .def("validate", [](const flowgen::GeneratorConfig& cfg) {
            std::string error;
            bool valid = cfg.validate(&error);
            if (!valid) {
                throw std::runtime_error("Config validation failed: " + error);
            }
            return valid;
        });

    // Stats binding
    py::class_<flowgen::FlowGenerator::Stats>(m, "GeneratorStats")
        .def_readonly("flows_generated", &flowgen::FlowGenerator::Stats::flows_generated)
        .def_readonly("elapsed_time_seconds", &flowgen::FlowGenerator::Stats::elapsed_time_seconds)
        .def_readonly("flows_per_second", &flowgen::FlowGenerator::Stats::flows_per_second)
        .def_readonly("current_timestamp_ns", &flowgen::FlowGenerator::Stats::current_timestamp_ns);

    // FlowGenerator binding (non-copyable, use std::unique_ptr holder)
    py::class_<flowgen::FlowGenerator, std::unique_ptr<flowgen::FlowGenerator>>(m, "FlowGenerator")
        .def(py::init<>())
        .def("initialize", &flowgen::FlowGenerator::initialize,
             "Initialize generator with configuration")
        .def("next", [](flowgen::FlowGenerator& gen) -> py::object {
            flowgen::FlowRecord flow;
            if (gen.next(flow)) {
                return py::cast(flow);
            } else {
                return py::none();
            }
        }, "Generate next flow record, returns None when done")
        .def("is_done", &flowgen::FlowGenerator::is_done,
             "Check if generation is complete")
        .def("reset", &flowgen::FlowGenerator::reset,
             "Reset generator to initial state")
        .def("get_stats", &flowgen::FlowGenerator::get_stats,
             "Get generator statistics")
        .def("flow_count", &flowgen::FlowGenerator::flow_count,
             "Get number of flows generated")
        .def("current_timestamp_ns", &flowgen::FlowGenerator::current_timestamp_ns,
             "Get current timestamp in nanoseconds")
        .def("__iter__", [](flowgen::FlowGenerator& gen) -> flowgen::FlowGenerator* {
            return &gen;
        }, py::return_value_policy::reference)
        .def("__next__", [](flowgen::FlowGenerator& gen) -> flowgen::FlowRecord {
            flowgen::FlowRecord flow;
            if (gen.next(flow)) {
                return flow;
            } else {
                throw py::stop_iteration();
            }
        });

    // Utility functions
    m.def("calculate_flows_per_second", &flowgen::utils::calculate_flows_per_second,
          "Calculate flows per second from bandwidth",
          py::arg("bandwidth_gbps"), py::arg("avg_packet_size"));

    m.def("ip_str_to_uint32", &flowgen::utils::ip_str_to_uint32,
          "Convert IPv4 string to uint32_t",
          py::arg("ip_str"));

    m.def("uint32_to_ip_str", &flowgen::utils::uint32_to_ip_str,
          "Convert uint32_t to IPv4 string",
          py::arg("ip"));

    m.def("random_ipv4", &flowgen::utils::random_ipv4,
          "Generate random IPv4 address string",
          py::arg("subnet") = "");

    m.def("random_ipv4_uint32", &flowgen::utils::random_ipv4_uint32,
          "Generate random IPv4 address as uint32_t",
          py::arg("subnet") = "");

    m.def("random_port", &flowgen::utils::random_port,
          "Generate random port number",
          py::arg("min") = 1024, py::arg("max") = 65535);

    m.def("seed_random", [](uint64_t seed) {
        flowgen::utils::Random::instance().seed(seed);
    }, "Seed the random number generator");
}
