// vi:sw=4:ts=4

#pragma once

#include <stdint.h>

/// The type of sensor data.
/** IMU_PROFILE_n is a statically known profile that's agreed upon by
	the server and firmware. */
enum sensor_data_type {

	/// IMU sensor data, with up to four different profiles.
	/** The data payload should be struct imu_accel_data or struct
	    imu_accel_gyro_data, depending on the profile. */
	SENSOR_DATA_IMU_PROFILE_0 = 0,
    SENSOR_DATA_IMU_PROFILE_1 = 1,
    SENSOR_DATA_IMU_PROFILE_2 = 2,
    SENSOR_DATA_IMU_PROFILE_3 = 3,

	/// Heart Rate (HRS) data.
	/** The data payload should be a single uint8_t, which is the
		number of detected peaks (_not_ the heart rate), starting at
		.timestamp lasting for .duration.  Note that if we remove the
		.duration field from sensor_data_header, we will need to
		change the data payload to include the duration. */
	SENSOR_DATA_HRS = 4,

	/// Battery level.
	/** The data payload should be a single uint8_t with range
		[0..255]; 255 represents a fully charged battery; 0 represents
		minimal battery for operation. */
	SENSOR_DATA_BATTERY = 5,

	/// GPS data, supplied by the phone.
	SENSOR_DATA_PHONE_GPS = 6,

	/// Sort some of sensor data from the iPhone M7.
    SENSOR_DATA_IPHONE_M7 = 7,

	/// Pedometer data from the iPhone, if it has an M7 co-processor.
    SENSOR_DATA_IPHONE_PEDOMETER = 8,

	/// Some sort of phone metadata, that the phone can supply to the server.
	/** e.g. Phone app version. */
	SENSOR_DATA_PHONE_METADATA = 9,

	/// Some sort of Band metadata, that either the phone or Band can
	/// supply to the server.
	/** e.g. Firmware version (maybe obtained by the phone through a
		BLE command). */
    SENSOR_DATA_BAND_METADATA = 10,

    /// Crashlog data. We may not want this since we may store
    /// crashlog data in a separate partition.
    SENSOR_DATA_CRASHLOG = 11,
};

/// The sensor data header, which should prepend all raw sensor data
/// before it's written to storage. */
/** The intent for this is to create a struct that can be used by both
	the Band and phone (and and other sensor devices) to record sensor
	data. The Band can use this to record IMU data; the phone can use
	the same struct format to record GPS data. The Band's struct
	packing rules and endianness determine the canonical format since
	it's the most resource-constrained sensor. (Yeah, that means
	little-endian... network endianness be damned.)*/
struct sensor_data_header {
    /// The type of sensor data.
    /** Note that this also doubles as a version number; if an unknown
		enum is encountered, that's effectively a new version, and
		_all_ the data in the rest of this struct should be treated as
		unknown. */
	enum sensor_data_type type:4;

    /// A monotonically increasing sequence number, from [0..16].
    /** The sequence number is necessary for the server (or phone) to
        reconstruct the packets. */
    uint8_t sequence_number:4;

    /// Number of seconds since Jan 1, 2014 (the "epoch").
	/** 32 bits is enough to last us 136+ years, i.e. until the year
		2150. If you wanted to pick a different number of bits for
		more efficient bit-packing, here's how many years we can last
		for N number of bits:

		~k % echo $(( (2**32) / (60*60*24*365.0) ))
        136.19251953323186
        ~k % echo $(( (2**31) / (60*60*24*365.0) ))
        68.09625976661593
        ~k % echo $(( (2**30) / (60*60*24*365.0) ))
        34.048129883307965
        ~k % echo $(( (2**29) / (60*60*24*365.0) ))
        17.024064941653982
        ~k % echo $(( (2**28) / (60*60*24*365.0) ))
        8.5120324708269912
	*/
    uint32_t timestamp;

	/// The duration of this packet, in seconds (rounded down).
	/** This definitely isn't _needed_, since you can obviously work
	    out the duration of any sensor data by looking at the data
	    type and the size of the data packet. However, having this
	    field lets the phone and server determine the duration without
	    requiring parsing of the sensor data itself, which could be
	    very handy, or necessary.  The uint8_t type limits this
	    duration to 256 seconds.  I think that's OK. */
    uint8_t duration;

	/// The size of the data packet, in bytes.
	/** This is a variable-length quantity (VLQ) unsigned integer; see
		<http://en.wikipedia.org/wiki/Variable-length_quantity> for
		info on this. The motivation for this is that we expect most
		sensor data to be <= 128 bytes (e.g. GPS, HRS, battery, ). The
		one important exception is IMU data, which can be up to 4K,
		and should thus take 16 bits for the size (2 bytes).

		There's some example code for how to do right now in the
		_imu_process() function in main.c. That functionality should
		definitely be moved out to a separate function,
		e.g. vlq_encode().

		Note that we may not wish to use a variable-length array here
		(technically a "C99 flexible array member") if it makes the
		implementation cumbersome. I elected to put it in this struct
		for now to make it clear that the size needs to be part of the
		sensor data header... implementation-wise, we can leave this
		out of the struct and perhaps use a separate array outside of
		the struct for the VLQ. */
	uint8_t size[];

	// The data payload should immediately follow.
} PACKED;



// HRS data

struct hrs_data {
	uint8_t peaks;
} PACKED;

/* [TODO]s:

   Magic signature (e.g. 0xdeadbeef) + checksum, to make it easy for
   the server to resync if there's data loss or corruption. Maybe the
   phone can add this, so the Band doesn't need to?

*/
