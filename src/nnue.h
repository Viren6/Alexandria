#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <immintrin.h>

constexpr int INPUT_WEIGHTS = 768;
constexpr int HIDDEN_SIZE = 1024;
using NNUEIndices = std::pair<std::size_t, std::size_t>;

struct Network {
    int16_t featureWeights[INPUT_WEIGHTS * HIDDEN_SIZE];
    int16_t featureBias[HIDDEN_SIZE];
    int16_t outputWeights[HIDDEN_SIZE * 2];
    int16_t outputBias;
};

extern Network net;

class NNUE {
public:
    using accumulator = std::array<std::array<int16_t, HIDDEN_SIZE>, 2>;

    void init(const char* file);
    void add(NNUE::accumulator& board_accumulator, const int piece, const int to);
    void update(NNUE::accumulator& board_accumulator, std::vector<NNUEIndices>& NNUEAdd, std::vector<NNUEIndices>& NNUESub);
    void addSub(NNUE::accumulator& board_accumulator, NNUEIndices add, NNUEIndices sub);
    void addSubSub(NNUE::accumulator& board_accumulator, NNUEIndices add, NNUEIndices sub1, NNUEIndices sub2);
    [[nodiscard]] int32_t flatten(const int16_t *acc, const int16_t *weights);
    [[nodiscard]] int32_t output(const NNUE::accumulator& board_accumulator, const bool whiteToMove);
    [[nodiscard]] NNUEIndices GetIndex(const int piece, const int square);
    #if defined(USE_AVX2)
    [[nodiscard]] int32_t horizontal_add(const __m256i sum);
    #endif
};
