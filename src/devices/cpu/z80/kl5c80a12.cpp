// license:BSD-3-Clause
// copyright-holders:AJR
/***************************************************************************

    Kawasaki Steel (Kawatetsu) KL5C80A12 CPU

    This is based on KC82, an MMU-enhanced version of KC80 (KL5C8400),
    Kawasaki's Z80-compatible core with an internal 16-bit architecture
    and significantly faster opcode timings, operating at up to 10 MHz
    (CLK = XIN/2).

    Important functional blocks:
    — MMU
    — USART (KP51) (unemulated)
    — 16-bit timer/counters (KP64, KP63) (KP64 unemulated)
    — 16-level interrupt controller (KP69)
    — Parallel ports (KP65, KP66) (unemulated)
    — 512-byte high-speed RAM
    — External bus interface unit (unemulated)

***************************************************************************/

#include "emu.h"
#include "kl5c80a12.h"
#include "kp63.h"

// device type definition
DEFINE_DEVICE_TYPE(KL5C80A12, kl5c80a12_device, "kl5c80a12", "Kawasaki Steel KL5C80A12")

static const z80_daisy_config pseudo_daisy_config[] = { { "kp69" }, { nullptr } };


//-------------------------------------------------
//  kl5c80a12_device - constructor
//-------------------------------------------------

kl5c80a12_device::kl5c80a12_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: kc82_device(mconfig, KL5C80A12, tag, owner, clock,
					address_map_constructor(FUNC(kl5c80a12_device::internal_ram), this),
					address_map_constructor(FUNC(kl5c80a12_device::internal_io), this))
	, m_kp69(*this, "kp69")
{
}


//-------------------------------------------------
//  internal_ram - map for high-speed internal RAM
//-------------------------------------------------

void kl5c80a12_device::internal_ram(address_map &map)
{
	map(0xffe00, 0xfffff).ram().share("ram");
}


//-------------------------------------------------
//  internal_io - map for internal I/O registers
//-------------------------------------------------

void kl5c80a12_device::internal_io(address_map &map)
{
	map(0x00, 0x07).mirror(0xff00).rw(FUNC(kl5c80a12_device::mmu_r), FUNC(kl5c80a12_device::mmu_w));
	map(0x20, 0x25).mirror(0xff00).rw("timerb", FUNC(kp63_3channel_device::read), FUNC(kp63_3channel_device::write));
	map(0x34, 0x34).mirror(0xff00).rw(m_kp69, FUNC(kp69_device::isrl_r), FUNC(kp69_device::lerl_pgrl_w));
	map(0x35, 0x35).mirror(0xff00).rw(m_kp69, FUNC(kp69_device::isrh_r), FUNC(kp69_device::lerh_pgrh_w));
	map(0x36, 0x36).mirror(0xff00).rw(m_kp69, FUNC(kp69_device::imrl_r), FUNC(kp69_device::imrl_w));
	map(0x37, 0x37).mirror(0xff00).rw(m_kp69, FUNC(kp69_device::imrh_r), FUNC(kp69_device::ivr_imrh_w));
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void kl5c80a12_device::device_start()
{
	kc82_device::device_start();

	m_kp69->add_to_state(*this, KP69_IRR);
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void kl5c80a12_device::device_reset()
{
	kc82_device::device_reset();
}


//-------------------------------------------------
//  device_add_mconfig - add device-specific
//  machine configuration
//-------------------------------------------------

void kl5c80a12_device::device_add_mconfig(machine_config &config)
{
	KP69(config, m_kp69);
	m_kp69->int_callback().set_inputline(*this, INPUT_LINE_IRQ0);

	kp63_3channel_device &timerb(KP63_3CHANNEL(config, "timerb", DERIVED_CLOCK(1, 2)));
	timerb.outs_callback<0>().set(m_kp69, FUNC(kp69_device::ir_w<13>));
	timerb.outs_callback<1>().set(m_kp69, FUNC(kp69_device::ir_w<14>));
	timerb.outs_callback<2>().set(m_kp69, FUNC(kp69_device::ir_w<15>));
}


//-------------------------------------------------
//  device_config_complete - perform any
//  operations now that the configuration is
//  complete
//-------------------------------------------------

void kl5c80a12_device::device_config_complete()
{
	set_daisy_config(pseudo_daisy_config);
}
