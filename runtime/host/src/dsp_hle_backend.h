#pragma once

#include <cstdint>

#include "dsp_adapter_c.h"

namespace bluewake::dsp
{
// Dolphin's high-level DSP (see dsp_hle_backend.cpp). One instance at a time.
class HleBackend
{
public:
  HleBackend();
  ~HleBackend();
  HleBackend(const HleBackend&) = delete;
  HleBackend& operator=(const HleBackend&) = delete;

  bool initialize(void* user, BluewakeDspGuestPointerFn guest_pointer, std::uint8_t* aram,
                  std::uint32_t aram_size, BluewakeDspTimebaseFn timebase,
                  BluewakeDspInterruptFn interrupt);
  int run_cycles(int cycles);
  void write_control(std::uint16_t value);
  std::uint16_t read_control();
  void write_cpu_mailbox(std::uint32_t value);
  std::uint32_t peek_cpu_mailbox();
  std::uint32_t peek_dsp_mailbox();
  std::uint16_t read_dsp_mailbox_low();
  std::uint64_t interrupts() const;

private:
  struct Impl;
  Impl* m_impl;
};
}  // namespace bluewake::dsp
