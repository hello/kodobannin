IMU
===

IMU stands for Inertial Motion (née Movement (ED: not Measurement?)) Unit. Basically: an
accelerometer, and optionally a gyroscope. IMU is the abstract term,
describing an accelerometer (or anything that detects movement) in
general. The actual chip is an Invensense MPU-6500.

Sleep/low-power modes
---------------------

The IMU can operate in two modes: sleep mode and wake mode; or
low-power mode and high-power/normal mode; or inactive mode and
active mode; cycle mode and continuous mode; all the same thing. The
MPU-6500 documentation typically refers to the modes as low-power
and normal modes.

The only difference between the two modes is that low-power mode:

1. Uses much less power, as you'd hope. (See section 3.3.1 on page 11
of the MPU-6500 Product Specification PDF for exact details on power
draw).

2. Enables the accelerometer only; i.e. no gyroscope.

3. Does not write data to the FIFO. Boo.

We run the MPU-6500 in low-power mode most of the time, and switch to
normal, high-power mode when motion is detected, so that we can
collect samples through the FIFO.  We put the chip back into low-power
mode when motion is no longer detected.

### Implementation notes

The MPU-6500 documentation is slightly confusing about how to enable
low-power mode. You do this by writing a 1 to the CYCLE bit in the
`PWR_MGMT_1` register (107).  That's all.

The `LP_ACCEL_ODR` register controls the sampling rate for low-power
mode, and AFAIK, that register does _not_ affect normal mode.  The
sample rate divider register (25) _may_ affect low-power mode; I am
not sure, so I set it to match the value in `LP_ACCEL_ODR` immediately
after setting `LP_ACCEL_ODR` before kicking the chip into low-power
mode.

Motion Detection ("Wake-On-Motion")
-----------------------------------

The MPU-6500 can send an interrupt when motion is detected that
exceeds a user-defined threshold. The MPU-6500 calls this
"wake-on-motion" or "motion detection" capability.

The Wake-On-Motion name is deceiving, because the MPU-6500 does _not_
"wake up" the chip when motion is detected. It does not even require
the chip to be in low-power mode to enable motion detection; the
interrupt can be triggered from either low-power mode or normal
mode. If you wish to wake up the chip after motion is detected, you
need to do that yourself; the interrupt being triggered does not do
that for you. It it thus much better to think about "Wake-On-Motion"
as Motion Detection instead.

The Product Specification PDF recommends using 184 Hz for the low-pass
filter (see below for details on the filter) for accurate motion
detection when it is enabled. I do not know why this is the case, but
I'm doing as I'm told, and it seems to work. (Famous last words.)

You would typically run the chip in low-power mode when performing
motion detection, simply because it uses less power. When choosing the
sampling rate for low-power mode, keep in mind that an extremely low
sampling rate (such as 0.49 Hz) will not be quick enough to detect
sudden bumps. This may or may not be OK, depending on what motion what
we need to record.

### MPU-6050 Compatibility Mode

Motion detection also has a "MPU-6050 compatibility mode" that is
exclusive to motion detection. (The MPU-6500 also has a general
"entire-chip" MPU-6050 compatibility mode that is _entirely separate_
from the motion detection's MPU-6050 compatibility mode. We are
talking about the latter here only.)  The MPU-6050 compatbility mode
changes how the chip internally performs motion detection: in MPU-6050
mode, the chip will take a single "reference" sample before motion
detection is activated, and compare subsequent samples to the
reference samples for motion detection. In the default MPU-6500 mode,
subsequent samples are compared with previous samples to detect
motion, which typically results in more accurate motion detection. We
use the default, smarter MPU-6500 mode.

### Implementation notes

Enabling motion detection is documented well in Section 5.1 of the
MPU-6500 Product Specification PDF. Of course, the order of steps
described there seems to differ from the order of steps taken by
Invensense's sample source code in `inv_mpu.c`.  I've used the order
in Section 5.1 of the Product Specification PDF and found that to
work well, so I've ignored the vendor's source code in this case. It
appears that the chip isn't very sensitive to the order: as long as
the registers are set correctly in whatever order, motion detection
will work.

To enable the motion detection's MPU-6050 compatibility mode, write a
non-zero value to the `LP_WAKE_CTRL` bits in register 108. We do not
do this, since we do not use MPU-6050 compatbility mode. You may also
be able to get this behavior by flipping the bit for the
`ACCEL_INTEL_MODE` in register 105. I'm not sure; the documentation
about this is sparse.

Interrupts
----------

The IMU has an interrupt pin that is designed to be wired up to a GPIO
pin on the host processor. The GPIO pin is defined as `IMU_INT` in
`device_params.h`.

The IMU is configured to be level-triggered, push-pull, latch-out, and
signals _low_ on an interrupt. You will need to clear the interrupt
with `imu_clear_interrupt_status()` or `imu_deactivate()` before the
IMU will re-trigger the interrupt.

The interupt can signal several events to the host processor, such as
wake-on-motion (explained above), FIFO overflow (explained below),
etc. You will need to read the interrupt status register yourself to
see exactly what event triggeed the interrupt.

To clear the interrupt, use `imu_clear_interrupt_status()`.

### Implementation notes

`imu_clear_interrupt_status()` clears the interrupt status by
_reading_ the interupt status register. That's all it does. You do not
need (or want) to write to the interrupt status register to clear
it. This is weird.

The MPU-6500 can also be configured to clear the interrupt status by
reading _any_ register, instead of the specific interrupt status
register. This is even weirder; don't do it.

Sample Rate
-----------

The IMU has two separate sampling rates: one for low-power mode, and
one for normal mode.

The sampling rate for normal mode has a maximum rate of 1000 Hz, but
can otherwise be any number that 1000 divides by evenly (e.g. 50 Hz
divides 1000 evenly by 20, so that's OK. 51 Hz is not possible).

The sampling rate in low-power mode must be chosen from a small, fixed
set of available sampling rates: 0.25 Hz, 0.49 Hz, 0.98 Hz, 1.95 Hz,
3.91 Hz, 7.81 Hz, 15.63 Hz, 31.25 Hz, 52.50 Hz, 125 Hz, 250 Hz, or 500
Hz.

We restrict the sampling rates for normal mode to be the set of
sampling rates for low-power mode, since there was a possibility that
we may always be able to run the chip in low-power mode if we do
this. Unfortunately, we've since found out that running the chip in
low-power mode never writes to the FIFO, which torpedos that idea. It
shouldn't be too difficult to change the sampling rate in normal mode
to be more flexible if we desire this (e.g. 25 Hz).

### Implementation notes

The reason for this odd-looking set of low-power sampling rates is
because the sampling interval for each of those rates is a
power-of-two multiple in milliseconds; the sampling interval for 0.25
Hz is 4096 ms, the sampling interval for 0.49 Hz is 2048 ms, 0.98 Hz
is 1024 Hz, etc. See the detailed comment at the start of
`imu_get_sampling_interval()` for more information.

Low-Pass Filter
---------------

The MPU-6500 has a low-pass filter (but no high-pass filter),
a.k.a. LPF, that can be optionally applied to the accelerometer and/or
gyroscope data, to smooth out outliers.  The accelerometer and
gyroscope have independent LPFs.

The LPF is typically configured to be operating at half of the sample
rate; e.g. if the sample rate is 50Hz, the LPF should be configured to
have an output bandwidth of 25 Hz. I'm guessing this makes sense since
the LPF then tracks the nyquist frequency of the accelerometer, though
I'm no signal processing expert, so I don't really know what this
means. (Not unusual...)

For motion detection, the Product Specification PDF recommends an LPF
output bandwidth of 184 Hz. I do not know why. So we do this when
motion detection is enabled, and we then reset the LPF to be 1/2 of
the sample rate after we wake up the chip when motion is detected.

FIFO
----

The MPU-6500 has a FIFO where it can dump sensor data. This enables
you to put your host processor to sleep for a while and read back
sensor data from the FIFO at a later time, instead of using the host
processor to poll the MPU-6500's registers to constantly read
values. This saves power.

Each FIFO value is two's complement signed 16-bit; i.e. a C `int16_t`
a.k.a. `unsigned short`.

The FIFO capacity defaults to 512 bytes, but can be configured to be
512 bytes, 1K, 2K or 4K. We configure it to 4K, via poking at
undocumented registers (see implementation notes below).

When the FIFO is full, the chip can be configured to either stop
collection of new data, or overwrite the oldest data with incoming
data. We've configured the FIFO to overwrite the oldest data.

The FIFO can send an interrupt when it's full. However, this is
pretty much useless, because by the time you've received the
interrupt, the FIFO is already full, so you're already missing data
unless your sampling rate is so slow that you can grab data from
the full FIFO before the next sample is collected.

I'm unsure if the FIFO will store all the accelerometer/gyroscope
values atomically. Ideally, if you have the accelerometer XYZ sensor
enabled, the FIFO size would only ever be a multiple of 6 bytes:
sizeof(int16_t) * 3 components (for each X/Y/Z axis). I do not think
this is true, so reading from the FIFO probably requires some logic
to ensure that you'd read multiples of 6 bytes at a time (the
"stride"), or 12 bytes if you have the gyroscope enabled.

The chip can also send an interrupt when "data is ready", i.e. the
first sample hits the FIFO. This also isn't very useful.

Ideally, we'd like an interrupt to be triggered when the FIFO hits a
certain watermark level, e.g. 75% full, so we can then merrily read
from it without any data being lost. This would be nice, to say the
least.

### Implementation notes

To clear the FIFO, I'm choosing to simply read it. You can also write
to the FIFO_RST bit in register 106 to "reset" it, but the
documentation is unclear about what "reset" means. The chip may
require a delay after the reset, or it may get the stride out-of-sync
with the values (so the FIFO data may now look like YZXYZXYZX instad
of XYZXYZXYZ). It seems safer to just read old data with a stride
multiple that we know will be correct.

To set the FIFO size, we write to bits 6 and 7 in register 29. This
is undocumented, and was reverse-engineered from the vendor's source
code.

Digital Motion Processor (DMP)
------------------------------

The MPU-6500 has an onboard host processor that you can upload some
firmware to. Invensense has some proprietary firmware that they call
the Digital Motion Processor, or DMP. The DMP adds extra features to
the MPU-6500, such as tap detection, automatic calibration,
pedometer functionality, and output in quaternions instead of raw
gyroscope/accelerometer values.  We do not use the DMP.

The memory for the DMP is _shared_ with the FIFO, so the DMP is
uploaded to FIFO memory. Enabling the DMP therefore shrinks the FIFO
capacity to a maximum of 1K.

The DMP is not well documented. Most of it is done through the
vendor source code.

### Implementation notes

There is a imu_dmp branch that has code to upload the DMP firmware
to the chip. As far as I know, the uploading code works and the DMP
can actually run, but nothing at all has been tested beyond that.
