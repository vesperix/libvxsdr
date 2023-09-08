// Copyright (c) 2023 Vesperix Corporation
// SPDX-License-Identifier: GPL-3.0-or-later

#include <iostream>
#include <chrono>
#include <thread>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>


int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "usage: test_sleep_resolution <test delay in seconds> <number of repetitions>" << std::endl;
        return -1;
    }

    double desired_delay = std::strtod(argv[1], nullptr);
    auto delay_ns = std::chrono::nanoseconds(std::llround(1e9 * desired_delay));

    unsigned n_reps = atoi(argv[2]);

    std::vector<double> delay(n_reps);
    std::vector<double> error(n_reps);

    for (unsigned n = 0; n < n_reps; n++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(delay_ns);
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> d = t1 - t0;
        delay[n] = d.count();
        error[n] = d.count() - desired_delay;
    }

    double mean_delay = std::reduce(delay.begin(), delay.end()) / n_reps;
    std::sort(delay.begin(), delay.end());

    double min_delay = delay.front();
    double max_delay = delay.back();
    double median_delay;
    if (n_reps < 2) {
        median_delay = 0;
    }
    else if (n_reps % 2 == 0) {
        median_delay = 0.5 * (delay[n_reps / 2 - 1] + delay[n_reps / 2]);
    } else {
        median_delay = delay[n_reps / 2];
    }

    std::cout << mean_delay << "  " << median_delay << "  " << min_delay << "  " << max_delay << std::endl;

    double mean_error = std::reduce(error.begin(), error.end()) / n_reps;
    std::sort(error.begin(), error.end());

    double min_error = error.front();
    double max_error = error.back();
    double median_error;
    if (n_reps < 2) {
        median_error = 0;
    }
    else if (n_reps % 2 == 0) {
        median_error = 0.5 * (error[n_reps / 2 - 1] + error[n_reps / 2]);
    } else {
        median_error = error[n_reps / 2];
    }

    std::cout << mean_error << "  " << median_error << "  " << min_error << "  " << max_error << std::endl;

    bool pass = (median_error <= 0.5 * desired_delay) and (min_error >= 0.0) and (max_error <= desired_delay);

    std::cout << (pass ? "passed" : "failed") << std::endl;

    return (pass ? 0 : 1);
}
