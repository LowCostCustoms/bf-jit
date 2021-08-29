#ifndef BFJIT_MIR_COMPILER_HPP
#define BFJIT_MIR_COMPILER_HPP

#include "types.hpp"

#include <memory>

namespace bfjit
{
class MirCompiler final : public CompilerBackend
{
public:
    MirCompiler();

    ~MirCompiler() override;

    MainFunc Compile(const CompilerContext &context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace bfjit

#endif // BFJIT_MIR_COMPILER_HPP
