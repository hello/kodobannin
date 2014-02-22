// vi:et:sw=4 ts=4

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
    little-endian... network endianness be damned.) */
struct sensor_data_header {
    /// This should always be 0x55AA.
    /** The signature assists the server in being able to detect a
        valid packet if it loses the stream (due to corrupted storage,
        etc). We use 0x55AA here because the bit pattern is
        101010110101010; the alternation of 0s and 1s, and the
        distance between 0x55 and 0xAA, makes it more unlikely for a
        real value of this to occur. */
    uint16_t signature;

    /// Data checksum.
    /** To calculate the checksum: fill out all the other fields in
        this struct, and set the value of this field to 0. Then, sum
        all bytes in this struct modulo 256, calculate 256 minus that
        sum, and write the result here. See memsum() in util.c for an
        implementation. */
    uint8_t checksum;

    /// The size of the data payload only (i.e. excluding this header).
    /** If you are a phone or server and encounter an unknown version
        number, you know exactly how many bytes to skip. 12 bits gives
        us a maximum of 4096 bytes for the data. */
    uint16_t size:12;

    /// The type of sensor data.
    /** Note that this also doubles as a version number; if an unknown
        enum is encountered, that's effectively a new version, and
        _all_ the data in the rest of this struct should be treated as
        unknown. */
    enum sensor_data_type type:4;

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

    // The data payload should immediately follow.
} PACKED;
/**< The current size of this struct is 10 bytes. */


// HRS data

struct hrs_data {
    uint8_t peaks;
} PACKED;
