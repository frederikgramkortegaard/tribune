#pragma once
#include <string>
#include <vector>

class MPCComputation {
public:
    virtual ~MPCComputation() = default;
    virtual std::string compute(const std::vector<std::string>& shards) = 0;
    virtual std::string getComputationType() const = 0;
};

class SumComputation : public MPCComputation {
public:
    std::string compute(const std::vector<std::string>& shards) override;
    std::string getComputationType() const override { return "sum"; }
};