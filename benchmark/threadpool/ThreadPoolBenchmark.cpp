#include <vector>
#include <future>
#include <thread>
#include <random>
#include <nanobench.h>
#include "ThreadPool.h"

using namespace server::threadpool;
using namespace std::chrono_literals;

ThreadPoolConfig Config = {4, std::thread::hardware_concurrency(), 0};
static ThreadPool pool = ThreadPool(Config);

void matrixMultiplication(int size) {
    std::vector<std::vector<int>> A(size, std::vector<int>(size));
    std::vector<std::vector<int>> B(size, std::vector<int>(size));
    std::vector<std::vector<int>> C(size, std::vector<int>(size, 0));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 10);

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            A[i][j] = dis(gen);
            B[i][j] = dis(gen);
        }
    }

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            for (int k = 0; k < size; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void benchmarkThreadPool(ThreadPool& pool, int numTasks, int matrixSize, std::string title) {
    ankerl::nanobench::Bench bench;
    bench.timeUnit(1ms, "ms");
    bench.title(title);
    bench.run("Matrix Multiplication", [&] {
        std::vector<std::future<bool>> futures;
        for (int i = 0; i < numTasks; ++i) {
            futures.push_back(pool.EnqueueTask([matrixSize] {
                matrixMultiplication(matrixSize);
                return true;
            }));
        }

        for (auto& future : futures) {
            future.get();
        }
    });
}

int main() {
    std::vector<std::pair<std::size_t, std::size_t>> args = {
        {8, 1000'00}, {64, 75'000}, {256, 50'000}, {512, 35'000}, {1024, 25'000}};
    for (const auto& [array_size, iterations] : args) {
        auto bench_title = std::string("matrix multiplication ") + std::to_string(array_size) +
                               "x" + std::to_string(array_size);

        benchmarkThreadPool(pool, array_size, iterations, bench_title);
    }

    return 0;
}
