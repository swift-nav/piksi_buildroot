/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

// pgrgich: need to handle invalid obs...

#include "rtcm3_decode.h"
#include "rtcm3_messages.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    rtcm_msg_header header;
    header.msg_num = 1001;
    header.div_free = 0;
    header.n_sat = 3;
    header.smooth = 0;
    header.stn_id = 7;
    header.sync = 1;
    header.tow = 309000;

    rtcm_obs_message msg1001;
    memset( (void*)&msg1001, 0, sizeof( msg1001 ) );
    msg1001.header = header;
    msg1001.sats[0].svId = 4;
    msg1001.sats[0].obs[0].code = 0;
    msg1001.sats[0].obs[0].pseudorange = 20000004.4;
    msg1001.sats[0].obs[0].carrier_phase = 105100794.4;
    msg1001.sats[0].obs[0].lock = 900;
    msg1001.sats[0].obs[0].flags.valid_pr = 1;
    msg1001.sats[0].obs[0].flags.valid_cp = 1;
    msg1001.sats[0].obs[0].flags.valid_lock = 1;

    msg1001.sats[1].svId = 6;
    msg1001.sats[1].obs[0].code = 0;
    msg1001.sats[1].obs[0].pseudorange = 22000004.4;
    msg1001.sats[1].obs[0].carrier_phase = 115610703.4;
    msg1001.sats[1].obs[0].lock = 254;
    msg1001.sats[1].obs[0].flags.valid_pr = 1;
    msg1001.sats[1].obs[0].flags.valid_cp = 1;
    msg1001.sats[1].obs[0].flags.valid_lock = 1;

    msg1001.sats[2].svId = 6;
    msg1001.sats[2].obs[0].code = 0;
    msg1001.sats[2].obs[0].pseudorange = 22000004.4;
    msg1001.sats[2].obs[0].carrier_phase = 115610553.4;
    msg1001.sats[2].obs[0].lock = 254;
    msg1001.sats[2].obs[0].flags.valid_pr = 1;
    msg1001.sats[2].obs[0].flags.valid_cp = 0;
    msg1001.sats[2].obs[0].flags.valid_lock = 0;

    u8 buff[1024];
    u16 size = rtcm3_encode_1001(&msg1001, buff );

    rtcm_obs_message msg1001_out;
    s8 ret = rtcm3_decode_1001( buff, &msg1001_out );

    assert( ret == 0 && msgobs_equals( &msg1001, &msg1001_out ) );

    rtcm_obs_message msg1002;
    msg1002 = msg1001;
    msg1002.header.msg_num = 1002;
    msg1002.sats[0].obs[0].cnr = 3.4;
    msg1002.sats[0].obs[0].flags.valid_cnr = 1;

    msg1002.sats[1].obs[0].cnr = 50.2;
    msg1002.sats[1].obs[0].flags.valid_cnr = 1;

    msg1002.sats[2].obs[0].cnr = 50.2;
    msg1002.sats[2].obs[0].flags.valid_cnr = 0;

    size = rtcm3_encode_1002(&msg1002, buff );

    rtcm_obs_message msg1002_out;
    ret = rtcm3_decode_1002( buff, &msg1002_out );

    assert( ret == 0 && msgobs_equals( &msg1002, &msg1002_out ) );

    rtcm_obs_message msg1003;
    msg1003 = msg1001;
    msg1003.header.msg_num = 1003;

    msg1003.sats[0].obs[1] = msg1003.sats[0].obs[0];
    msg1003.sats[0].obs[1].pseudorange = 20000124.4;
    msg1003.sats[0].obs[1].carrier_phase = 81897184.4;

    msg1003.sats[1].obs[1] = msg1003.sats[1].obs[0];
    msg1003.sats[1].obs[1].pseudorange = 22000024.4;
    msg1003.sats[1].obs[1].carrier_phase = 90086422.236;

    size = rtcm3_encode_1003(&msg1003, buff );

    rtcm_obs_message msg1003_out;
    ret = rtcm3_decode_1003( buff, &msg1003_out );

    assert( ret == 0 && msgobs_equals( &msg1003, &msg1003_out ) );

    rtcm_obs_message msg1004;
    msg1004 = msg1003;
    msg1004.header.msg_num = 1004;

    msg1004.sats[0].obs[0].cnr = 3.4;
    msg1004.sats[0].obs[0].flags.valid_cnr = 1;
    msg1004.sats[0].obs[1].cnr = 1.4;
    msg1004.sats[0].obs[1].flags.valid_cnr = 1;

    msg1004.sats[1].obs[0].cnr = 50.2;
    msg1004.sats[1].obs[0].flags.valid_cnr = 1;
    msg1004.sats[1].obs[1].cnr = 50.2;
    msg1004.sats[1].obs[1].flags.valid_cnr = 1;

    msg1004.sats[2].obs[0].cnr = 50.2;
    msg1004.sats[2].obs[0].flags.valid_cnr = 0;
    msg1004.sats[2].obs[1].cnr = 54.2;
    msg1004.sats[2].obs[1].flags.valid_cnr = 1;

    size = rtcm3_encode_1004(&msg1004, buff );

    rtcm_obs_message msg1004_out;
    ret = rtcm3_decode_1004( buff, &msg1004_out );

    assert( ret == 0 && msgobs_equals( &msg1004, &msg1004_out ) );

    rtcm_msg_1005 msg1005;

    msg1005.stn_id = 5;
    msg1005.ref_stn_ind = 1;
    msg1005.quart_cycle_ind = 1;
    msg1005.osc_ind = 0;
    msg1005.ITRF = 1;
    msg1005.GPS_ind = 1;
    msg1005.GLO_ind = 1;
    msg1005.GAL_ind = 0;
    msg1005.arp_x = 3578346.5475;
    msg1005.arp_y = -5578346.5578;
    msg1005.arp_z = 2578346.6757;

    size = rtcm3_encode_1005(&msg1005, buff );

    rtcm_msg_1005 msg1005_out;
    ret = rtcm3_decode_1005( buff, &msg1005_out );

    assert( ret == 0 && msg1005_equals( &msg1005, &msg1005_out ) );

    rtcm_msg_1006 msg1006;

    msg1006.msg_1005.stn_id = 5;
    msg1006.msg_1005.ref_stn_ind = 0;
    msg1006.msg_1005.quart_cycle_ind = 0;
    msg1006.msg_1005.osc_ind = 1;
    msg1006.msg_1005.ITRF = 0;
    msg1006.msg_1005.GPS_ind = 0;
    msg1006.msg_1005.GLO_ind = 0;
    msg1006.msg_1005.GAL_ind = 1;
    msg1006.msg_1005.arp_x = 3573346.5475;
    msg1006.msg_1005.arp_y = -5576346.5578;
    msg1006.msg_1005.arp_z = 2578376.6757;
    msg1006.ant_height = 1.567;

    size = rtcm3_encode_1006(&msg1006, buff );

    rtcm_msg_1006 msg1006_out;
    ret = rtcm3_decode_1006( buff, &msg1006_out );

    assert( ret == 0 && msg1006_equals( &msg1006, &msg1006_out ) );

    rtcm_msg_1007 msg1007;

    msg1007.stn_id = 1022;
    msg1007.desc_count = 29;
    strcpy( msg1007.desc, "Something with 29 characters." );
    msg1007.ant_id = 254;

    size = rtcm3_encode_1007(&msg1007, buff );

    rtcm_msg_1007 msg1007_out;
    ret = rtcm3_decode_1007( buff, &msg1007_out );

    assert( ret == 0 && msg1007_equals( &msg1007, &msg1007_out ) );

    rtcm_msg_1008 msg1008;

    msg1008.msg_1007.stn_id = 22;
    msg1008.msg_1007.desc_count = 27;
    strcpy( msg1008.msg_1007.desc, "Something without 30 chars." );
    msg1008.msg_1007.ant_id = 1;
    msg1008.serial_count = 9;
    strcpy( msg1008.serial_num, "123456789" );

    size = rtcm3_encode_1008(&msg1008, buff );

    rtcm_msg_1008 msg1008_out;
    ret = rtcm3_decode_1008( buff, &msg1008_out );

    assert( ret == 0 && msg1008_equals( &msg1008, &msg1008_out ) );
}

bool msgobs_equals( const rtcm_obs_message *msg_in, const rtcm_obs_message *msg_out ) {
    if( msg_in->header.msg_num != msg_out->header.msg_num ) {
        return false;
    }
    if( msg_in->header.stn_id != msg_out->header.stn_id ) {
        return false;
    }
    if( msg_in->header.tow != msg_out->header.tow ) {
        return false;
    }
    if( msg_in->header.sync != msg_out->header.sync ) {
        return false;
    }

    u8 num_sats = 0;
    for (u8 i=0; i<msg_in->header.n_sat; i++) {
        flag_bf l1_flags = msg_in->sats[i].obs[L1_FREQ].flags;
        flag_bf l2_flags = msg_in->sats[i].obs[L2_FREQ].flags;
        if( l1_flags.valid_pr && l1_flags.valid_cp && ( msg_in->header.msg_num == 1001 || msg_in->header.msg_num == 1002 || ( l2_flags.valid_pr && l2_flags.valid_cp ) ) ) {
            ++num_sats;
        }
    }
    if( num_sats != msg_out->header.n_sat ) {
        return false;
    }
    if( msg_in->header.div_free != msg_out->header.div_free ) {
        return false;
    }
    if( msg_in->header.smooth != msg_out->header.smooth ) {
        return false;
    }

    u8 out_sat_idx = 0;
    for( u8 in_sat_idx = 0; in_sat_idx < msg_in->header.n_sat; ++in_sat_idx ) {
        flag_bf l1_flags = msg_in->sats[in_sat_idx].obs[L1_FREQ].flags;
        flag_bf l2_flags = msg_in->sats[in_sat_idx].obs[L2_FREQ].flags;
        if( !l1_flags.valid_pr || !l1_flags.valid_cp || ( !msg_in->header.msg_num == 1001 && !msg_in->header.msg_num == 1002 && ( l2_flags.valid_pr || l2_flags.valid_cp ) ) ) {
            continue;
        }

        if( msg_in->sats[in_sat_idx].svId != msg_out->sats[out_sat_idx].svId ) {
            return false;
        }

        u8 amb = 0;
        if( msg_in->header.msg_num == 1001 || msg_in->header.msg_num == 1003 ) {
            amb = (u8)roundl(( msg_in->sats[in_sat_idx].obs[0].pseudorange - msg_out->sats[out_sat_idx].obs[0].pseudorange ) / PRUNIT_GPS);
        }

        for( u8 freq = 0; freq < NUM_FREQS; ++freq ) {
            const rtcm_freq_data *in_freq = &msg_in->sats[in_sat_idx].obs[freq];
            const rtcm_freq_data *out_freq = &msg_out->sats[out_sat_idx].obs[freq];

            if( in_freq->flags.valid_pr != out_freq->flags.valid_pr ) {
                return false;
            }

            if( in_freq->flags.valid_cp != out_freq->flags.valid_cp ) {
                return false;
            }

            if( ( msg_in->header.msg_num == 1002 || msg_in->header.msg_num == 1004 ) && in_freq->flags.valid_cnr != out_freq->flags.valid_cnr ) {
                return false;
            }

            if( in_freq->flags.valid_lock != out_freq->flags.valid_lock ) {
                return false;
            }

            if( in_freq->flags.valid_pr ) {
                if( in_freq->code != out_freq->code || fabs( in_freq->pseudorange - out_freq->pseudorange - amb * PRUNIT_GPS ) > 0.01 ) {
                    return false;
                }
            }
            if( in_freq->flags.valid_cp ) {
                if( fabs( in_freq->carrier_phase - out_freq->carrier_phase - ((double)amb * PRUNIT_GPS / (CLIGHT / FREQS[freq])) ) > 0.0005 / (CLIGHT / FREQS[freq])   ) {
                    return false;
                }
            }
            if( in_freq->flags.valid_cnr ) {
                if( fabs( in_freq->cnr - out_freq->cnr ) > 0.125 ) {
                    return false;
                }
            }
            if( in_freq->flags.valid_lock ) {
//                if( in_freq->lock < 24 ) {
//                    if( out_freq->lock < 0 || out_freq->lock >= 24 ) {
//                        return false;
//                    }
//                }
//                else if( in_freq->lock < 72 ) {
//                    if( out_freq->lock < 24 || out_freq->lock >= 72 ) {
//                        return false;
//                    }
//                }
//                else if( in_freq->lock < 168 ) {
//                    if (out_freq->lock < 72 || out_freq->lock >= 168) {
//                        return false;
//                    }
//                }
//                else if( in_freq->lock < 360 ) {
//                    if( out_freq->lock < 168 || out_freq->lock >= 360 ) {
//                        return false;
//                    }
//                }
//                else if( in_freq->lock < 744 ) {
//                    if( out_freq->lock < 360 || out_freq->lock >= 744 ) {
//                        return false;
//                    }
//                }
//                else if( in_freq->lock < 937 ) {
//                    if( out_freq->lock < 744 || out_freq->lock >= 937 ) {
//                        return false;
//                    }
//                }
//                else {
//                    if( out_freq->lock < 937 ) {
//                        return false;
//                    }
//                }
            }
        }
        ++out_sat_idx;
    }

    return true;
}

bool msg1005_equals( const rtcm_msg_1005 *lhs, const rtcm_msg_1005 *rhs ) {
    if( lhs->stn_id != rhs->stn_id ) {
        return false;
    }
    if( lhs->ITRF != rhs->ITRF ) {
        return false;
    }
    if( lhs->GPS_ind != rhs->GPS_ind ) {
        return false;
    }
    if( lhs->GLO_ind != rhs->GLO_ind ) {
        return false;
    }
    if( lhs->GAL_ind != rhs->GAL_ind ) {
        return false;
    }
    if( lhs->ref_stn_ind != rhs->ref_stn_ind ) {
        return false;
    }
    if( lhs->osc_ind != rhs->osc_ind ) {
        return false;
    }
    if( lhs->quart_cycle_ind != rhs->quart_cycle_ind ) {
        return false;
    }
    if( fabs( lhs->arp_x - rhs->arp_x ) > 0.00005 ) {
        return false;
    }
    if( fabs( lhs->arp_y - rhs->arp_y ) > 0.00005 ) {
        return false;
    }
    if( fabs( lhs->arp_z - rhs->arp_z ) > 0.00005 ) {
        return false;
    }

    return true;
}

bool msg1006_equals( const rtcm_msg_1006 *lhs, const rtcm_msg_1006 *rhs ) {
    if( fabs( lhs->ant_height - rhs->ant_height ) > 0.00005 ) {
        return false;
    }

    return msg1005_equals( &lhs->msg_1005, &rhs->msg_1005 );
}

bool msg1007_equals( const rtcm_msg_1007 *lhs, const rtcm_msg_1007 *rhs ) {
    if( lhs->stn_id != rhs->stn_id ) {
        return false;
    }
    if( lhs->desc_count != rhs->desc_count ) {
        return false;
    }
    for( u8 ch = 0; ch < lhs->desc_count; ++ch ) {
        if( lhs->desc[ch] != rhs->desc[ch] ) {
            return false;
        }
    }
    if( lhs->ant_id != rhs->ant_id ) {
        return false;
    }

    return true;
}

bool msg1008_equals( const rtcm_msg_1008 *lhs, const rtcm_msg_1008 *rhs ) {
    for( u8 ch = 0; ch < lhs->serial_count; ++ch ) {
        if( lhs->serial_num[ch] != rhs->serial_num[ch] ) {
            return false;
        }
    }

    return msg1007_equals( &lhs->msg_1007, &rhs->msg_1007 );
}

/** Get bit field from buffer as an unsigned integer.
 * Unpacks `len` bits at bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \return Bit field as an unsigned value.
 */
u32 getbitu(const u8 *buff, u32 pos, u8 len)
{
    u32 bits = 0;

    for (u32 i = pos; i < pos + len; i++) {
        bits = (bits << 1) +
               ((buff[i/8] >> (7 - i%8)) & 1u);
    }

    return bits;
}

/** Get bit field from buffer as an unsigned integer.
 * Unpacks `len` bits at bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \return Bit field as an unsigned value.
 */
u64 getbitul(const u8 *buff, u32 pos, u8 len)
{
    u64 bits = 0;

    for (u32 i = pos; i < pos + len; i++) {
        bits = (bits << 1) +
               ((buff[i/8] >> (7 - i%8)) & 1u);
    }

    return bits;
}

/** Get bit field from buffer as a signed integer.
 * Unpacks `len` bits at bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * This function sign extends the `len` bit field to a signed 32 bit integer.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \return Bit field as a signed value.
 */
s32 getbits(const u8 *buff, u32 pos, u8 len)
{
    s32 bits = (s32)getbitu(buff, pos, len);

    /* Sign extend, taken from:
     * http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
     */
    s32 m = 1u << (len - 1);
    return (bits ^ m) - m;
}

/** Get bit field from buffer as a signed integer.
 * Unpacks `len` bits at bit position `pos` from the start of the buffer.
 * Maximum bit field length is 64 bits, i.e. `len <= 64`.
 *
 * This function sign extends the `len` bit field to a signed 64 bit integer.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \return Bit field as a signed value.
 */
s64 getbitsl(const u8 *buff, u32 pos, u8 len)
{
    s64 bits = (s64)getbitul(buff, pos, len);

    /* Sign extend, taken from:
     * http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
     */
    s64 m = ((u64)1) << (len - 1);
    return (bits ^ m) - m;
}

/** Set bit field in buffer from an unsigned integer.
 * Packs `len` bits into bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \param data Unsigned integer to be packed into bit field.
 */
void setbitu(u8 *buff, u32 pos, u32 len, u32 data)
{
    u32 mask = 1u << (len - 1);

    if (len <= 0 || 32 < len)
        return;

    for (u32 i = pos; i < pos + len; i++, mask >>= 1) {
        if (data & mask)
            buff[i/8] |= 1u << (7 - i % 8);
        else
            buff[i/8] &= ~(1u << (7 - i % 8));
    }
}

/** Set bit field in buffer from an unsigned integer.
 * Packs `len` bits into bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \param data Unsigned integer to be packed into bit field.
 */
void setbitul(u8 *buff, u32 pos, u32 len, u64 data)
{
    u64 mask = ((u64)1) << (len - 1);

    if (len <= 0 || 64 < len)
        return;

    for (u32 i = pos; i < pos + len; i++, mask >>= 1) {
        if (data & mask)
            buff[i/8] |= ((u64)1) << (7 - i % 8);
        else
            buff[i/8] &= ~(((u64)1) << (7 - i % 8));
    }
}

/** Set bit field in buffer from a signed integer.
 * Packs `len` bits into bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \param data Signed integer to be packed into bit field.
 */
void setbits(u8 *buff, u32 pos, u32 len, s32 data)
{
    setbitu(buff, pos, len, (u32)data);
}

/** Set bit field in buffer from a signed integer.
 * Packs `len` bits into bit position `pos` from the start of the buffer.
 * Maximum bit field length is 32 bits, i.e. `len <= 32`.
 *
 * \param buff
 * \param pos Position in buffer of start of bit field in bits.
 * \param len Length of bit field in bits.
 * \param data Signed integer to be packed into bit field.
 */
void setbitsl(u8 *buff, u32 pos, u32 len, s64 data)
{
    setbitul(buff, pos, len, (u64)data);
}

void rtcm3_decode_frame(const uint8_t *frame, uint32_t frame_length)
{
  static uint32_t count = 0;
  uint16_t message_type = (frame[3] << 4) | ((frame[4] >> 4) & 0xf);
  printf("message type: %u, length: %u, count: %u\n",
          message_type, frame_length, ++count);
}

/** Write RTCM header for observation message types 1001..1004.
 *
 * The data message header will be written starting from byte zero of the
 * buffer. If the buffer also contains a frame header then be sure to pass a
 * pointer to the start of the data message rather than a pointer to the start
 * of the frame buffer. The RTCM observation header is 8 bytes (64 bits) long.
 *
 * If the Synchronous GNSS Message Flag is set to `0`, it means that no further
 * GNSS observables referenced to the same Epoch Time will be transmitted. This
 * enables the receiver to begin processing the data immediately after decoding
 * the message. If it is set to `1`, it means that the next message will
 * contain observables of another GNSS source referenced to the same Epoch
 * Time.
 *
 * Divergence-free Smoothing Indicator values:
 *
 * Indicator | Meaning
 * --------- | ----------------------------------
 *     0     | Divergence-free smoothing not used
 *     1     | Divergence-free smoothing used
 *
 * GPS Smoothing Interval indicator values are listed in RTCM 10403.1 Table
 * 3.4-4, reproduced here:
 *
 * Indicator | Smoothing Interval
 * --------- | ------------------
 *  000 (0)  |   No smoothing
 *  001 (1)  |   < 30 s
 *  010 (2)  |   30-60 s
 *  011 (3)  |   1-2 min
 *  100 (4)  |   2-4 min
 *  101 (5)  |   4-8 min
 *  110 (6)  |   >8 min
 *  111 (7)  |   Unlimited
 *
 * \param buff A pointer to the RTCM data message buffer.
 * \param type Message type number, i.e. 1001..1004 (DF002).
 * \param id Reference station ID (DF003).
 * \param t GPS time of epoch (DF004).
 * \param sync Synchronous GNSS Flag (DF005).
 * \param n_sat Number of GPS satellites included in the message (DF006).
 * \param div_free GPS Divergence-free Smoothing Indicator (DF007).
 * \param smooth GPS Smoothing Interval indicator (DF008).
 */
void rtcm3_write_header( const rtcm_msg_header *header, u8 num_sats, u8 *buff )
{
  setbitu(buff, 0, 12, header->msg_num);
  setbitu(buff, 12, 12, header->stn_id);
  setbitu(buff, 24, 30, (u32)round(header->tow*1e3));
  setbitu(buff, 54, 1, header->sync);
  setbitu(buff, 55, 5, num_sats);
  setbitu(buff, 60, 1, header->div_free);
  setbitu(buff, 61, 3, header->smooth);
}

/** Read RTCM header for observation message types 1001..1004.
 *
 * The data message header will be read starting from byte zero of the
 * buffer. If the buffer also contains a frame header then be sure to pass a
 * pointer to the start of the data message rather than a pointer to the start
 * of the frame buffer. The RTCM observation header is 8 bytes (64 bits) long.
 *
 * All return values are written into the parameters passed by reference.
 *
 * \param buff A pointer to the RTCM data message buffer.
 * \param type Message type number, i.e. 1001..1004 (DF002).
 * \param id Reference station ID (DF003).
 * \param tow GPS time of week of the epoch (DF004).
 * \param sync Synchronous GNSS Flag (DF005).
 * \param n_sat Number of GPS satellites included in the message (DF006).
 * \param div_free GPS Divergence-free Smoothing Indicator (DF007).
 * \param smooth GPS Smoothing Interval indicator (DF008).
 */
void rtcm3_read_header(const u8 *buff, rtcm_msg_header *header )
{
  header->msg_num = getbitu(buff, 0, 12);
  header->stn_id = getbitu(buff, 12, 12);
  header->tow = getbitu(buff, 24, 30) / 1e3;
  header->sync = getbitu(buff, 54, 1);
  header->n_sat = getbitu(buff, 55, 5);
  header->div_free = getbitu(buff, 60, 1);
  header->smooth = getbitu(buff, 61, 3);
}

/** Convert a lock time in seconds into a RTCMv3 Lock Time Indicator value.
 * See RTCM 10403.1, Table 3.4-2.
 *
 * \param time Lock time in seconds.
 * \return Lock Time Indicator value.
 */
static u8 to_lock_ind(u32 time)
{
  if (time < 24)
    return time;
  if (time < 72)
    return (time + 24) / 2;
  if (time < 168)
    return (time + 120) / 4;
  if (time < 360)
    return (time + 408) / 8;
  if (time < 744)
    return (time + 1176) / 16;
  if (time < 937)
    return (time + 3096) / 32;
  return 127;
}

/** Convert a RTCMv3 Lock Time Indicator value into a minimum lock time in seconds.
 * See RTCM 10403.1, Table 3.4-2.
 *
 * \param lock Lock Time Indicator value.
 * \return Minimum lock time in seconds.
 */
static u32 from_lock_ind(u8 lock)
{
  if (lock < 24)
    return lock;
  if (lock < 48)
    return 2*lock - 24;
  if (lock < 72)
    return 4*lock - 120;
  if (lock < 96)
    return 8*lock - 408;
  if (lock < 120)
    return 16*lock - 1176;
  if (lock < 127)
    return 32*lock - 3096;
  return 937;
}

u16 rtcm3_encode_1001(const rtcm_obs_message *rtcm_msg_1001, u8 *buff )
{
  u16 bit = 64; /* Start at end of header. */

  u8 num_sats = 0;
  for (u8 i=0; i<rtcm_msg_1001->header.n_sat; i++) {
      if( rtcm_msg_1001->sats[i].obs[L1_FREQ].flags.valid_pr && rtcm_msg_1001->sats[i].obs[L1_FREQ].flags.valid_cp ) {
          setbitu(buff, bit, 6, rtcm_msg_1001->sats[i].svId); bit += 6;
          encode_basic_freq_data( &rtcm_msg_1001->sats[i].obs[L1_FREQ], L1_FREQ, &rtcm_msg_1001->sats[i].obs[L1_FREQ].pseudorange, buff, &bit );
          ++num_sats;
      }
  }

  rtcm3_write_header( &rtcm_msg_1001->header, num_sats, buff );

    /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

/** Encode an RTCMv3 message type 1002 (Extended L1-Only GPS RTK Observables)
 * Message type 1002 has length `64 + n_sat*74` bits. Returned message length
 * is rounded up to the nearest whole byte.
 *
 * \param buff A pointer to the RTCM data message buffer.
 * \param id Reference station ID (DF003).
 * \param t GPS time of epoch (DF004).
 * \param n_sat Number of GPS satellites included in the message (DF006).
 * \param nm Struct containing the observation.
 * \param sync Synchronous GNSS Flag (DF005).
 * \return The message length in bytes.
 */
u16 rtcm3_encode_1002(const rtcm_obs_message *rtcm_msg_1002, u8 *buff )
{
  u16 bit = 64; /* Start at end of header. */

  u8 num_sats = 0;
  for (u8 i=0; i<rtcm_msg_1002->header.n_sat; i++) {
      if( rtcm_msg_1002->sats[i].obs[L1_FREQ].flags.valid_pr && rtcm_msg_1002->sats[i].obs[L1_FREQ].flags.valid_cp ) {
          setbitu(buff, bit, 6, rtcm_msg_1002->sats[i].svId);
          bit += 6;
          encode_basic_freq_data(&rtcm_msg_1002->sats[i].obs[L1_FREQ], L1_FREQ,
                                 &rtcm_msg_1002->sats[i].obs[L1_FREQ].pseudorange, buff, &bit);

          /* Calculate GPS Integer L1 Pseudorange Modulus Ambiguity (DF014). */
          u8 amb = (u8)(rtcm_msg_1002->sats[i].obs[L1_FREQ].pseudorange / PRUNIT_GPS);

          setbitu(buff, bit, 8, amb);
          bit += 8;
          setbitu(buff, bit, 8, (u8) roundl(rtcm_msg_1002->sats[i].obs[L1_FREQ].cnr * 4.0));
          bit += 8;
          ++num_sats;
      }
  }

    rtcm3_write_header( &rtcm_msg_1002->header, num_sats, buff );

    /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

u16 rtcm3_encode_1003(const rtcm_obs_message *rtcm_msg_1003, u8 *buff )
{
  u16 bit = 64; /* Start at end of header. */

  u8 num_sats = 0;
  for (u8 i=0; i<rtcm_msg_1003->header.n_sat; i++) {
      flag_bf l1_flags = rtcm_msg_1003->sats[i].obs[L1_FREQ].flags;
      flag_bf l2_flags = rtcm_msg_1003->sats[i].obs[L2_FREQ].flags;
      if( l1_flags.valid_pr && l1_flags.valid_cp && l2_flags.valid_pr && l2_flags.valid_cp ) {
          setbitu(buff, bit, 6, rtcm_msg_1003->sats[i].svId);
          bit += 6;
          encode_basic_freq_data(&rtcm_msg_1003->sats[i].obs[L1_FREQ], L1_FREQ,
                                 &rtcm_msg_1003->sats[i].obs[L1_FREQ].pseudorange, buff, &bit);
          encode_basic_freq_data(&rtcm_msg_1003->sats[i].obs[L2_FREQ], L2_FREQ,
                                 &rtcm_msg_1003->sats[i].obs[L1_FREQ].pseudorange, buff, &bit);
          ++num_sats;
      }
  }

  rtcm3_write_header( &rtcm_msg_1003->header, num_sats, buff );

    /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

u16 rtcm3_encode_1004(const rtcm_obs_message *rtcm_msg_1004, u8 *buff )
{
  u16 bit = 64; /* Start at end of header. */

  u8 num_sats = 0;
  for (u8 i=0; i<rtcm_msg_1004->header.n_sat; i++) {
      flag_bf l1_flags = rtcm_msg_1004->sats[i].obs[L1_FREQ].flags;
      flag_bf l2_flags = rtcm_msg_1004->sats[i].obs[L2_FREQ].flags;
      if( l1_flags.valid_pr && l1_flags.valid_cp && l2_flags.valid_pr && l2_flags.valid_cp ) {
          setbitu(buff, bit, 6, rtcm_msg_1004->sats[i].svId);
          bit += 6;
          encode_basic_freq_data(&rtcm_msg_1004->sats[i].obs[L1_FREQ], L1_FREQ,
                                 &rtcm_msg_1004->sats[i].obs[L1_FREQ].pseudorange, buff, &bit);

          /* Calculate GPS Integer L1 Pseudorange Modulus Ambiguity (DF014). */
          u8 amb = (u8)(rtcm_msg_1004->sats[i].obs[L1_FREQ].pseudorange / PRUNIT_GPS);

          setbitu(buff, bit, 8, amb);
          bit += 8;
          setbitu(buff, bit, 8, (u8) roundl(rtcm_msg_1004->sats[i].obs[L1_FREQ].cnr * 4.0));
          bit += 8;

          encode_basic_freq_data(&rtcm_msg_1004->sats[i].obs[L2_FREQ], L2_FREQ,
                                 &rtcm_msg_1004->sats[i].obs[L1_FREQ].pseudorange, buff, &bit);
          setbitu(buff, bit, 8, (u8) roundl(rtcm_msg_1004->sats[i].obs[L2_FREQ].cnr * 4.0));
          bit += 8;
          ++num_sats;
      }
  }

  rtcm3_write_header( &rtcm_msg_1004->header, num_sats, buff );

  /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

u16 rtcm3_encode_1005_base(const rtcm_msg_1005 *rtcm_msg_1005, u8 *buff, u16 *bit )
{
  setbitu(buff, *bit, 12, rtcm_msg_1005->stn_id); *bit += 12;
  setbitu(buff, *bit, 6, rtcm_msg_1005->ITRF); *bit += 6;
  setbitu(buff, *bit, 1, rtcm_msg_1005->GPS_ind); *bit += 1;
  setbitu(buff, *bit, 1, rtcm_msg_1005->GLO_ind); *bit += 1;
  setbitu(buff, *bit, 1, rtcm_msg_1005->GAL_ind); *bit += 1;
  setbitu(buff, *bit, 1, rtcm_msg_1005->ref_stn_ind); *bit += 1;
  setbitsl(buff, *bit, 38,  (s64)roundl( rtcm_msg_1005->arp_x * 10000.0 )); *bit += 38;
  setbitu(buff, *bit, 1, rtcm_msg_1005->osc_ind); *bit += 1;
  setbitu(buff, *bit, 1, 0); *bit += 1;
  setbitsl(buff, *bit, 38,  (s64)roundl( rtcm_msg_1005->arp_y * 10000.0 )); *bit += 38;
  setbitu(buff, *bit, 2, rtcm_msg_1005->quart_cycle_ind); *bit += 2;
  setbitsl(buff, *bit, 38,  (s64)roundl( rtcm_msg_1005->arp_z * 10000.0 )); *bit += 38;

  /* Round number of bits up to nearest whole byte. */
  return (*bit + 7) / 8;
}

u16 rtcm3_encode_1005(const rtcm_msg_1005 *rtcm_msg_1005, u8 *buff )
{
  u16 bit = 0;
  setbitu(buff, bit, 12, 1005); bit += 12;
  return rtcm3_encode_1005_base( rtcm_msg_1005, buff, &bit );
}

u16 rtcm3_encode_1006(const rtcm_msg_1006 *rtcm_msg_1006, u8 *buff )
{
  u16 bit = 0;
  setbitu(buff, bit, 12, 1006); bit += 12;
  rtcm3_encode_1005_base(&rtcm_msg_1006->msg_1005, buff, &bit );
  setbitu(buff, bit, 16, (u16)roundl(rtcm_msg_1006->ant_height*10000.0)); bit += 16;

  /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

u16 rtcm3_encode_1007_base(const rtcm_msg_1007 *rtcm_msg_1007, u8 *buff, u16 *bit )
{
  setbitu(buff, *bit, 12, rtcm_msg_1007->stn_id); *bit += 12;
  setbitu(buff, *bit, 8, rtcm_msg_1007->desc_count); *bit += 8;
  for( u8 i =0; i < rtcm_msg_1007->desc_count; ++i ) {
    setbitu(buff, *bit, 8, rtcm_msg_1007->desc[i]); *bit += 8;
  }
  setbitu(buff, *bit, 8, rtcm_msg_1007->ant_id); *bit += 8;

  /* Round number of bits up to nearest whole byte. */
  return (*bit + 7) / 8;
}

u16 rtcm3_encode_1007(const rtcm_msg_1007 *rtcm_msg_1007, u8 *buff )
{
  u16 bit = 0;
  setbitu(buff, bit, 12, 1007); bit += 12;
  return rtcm3_encode_1007_base( rtcm_msg_1007, buff, &bit );
}

u16 rtcm3_encode_1008(const rtcm_msg_1008 *rtcm_msg_1008, u8 *buff )
{
  u16 bit = 0;
  setbitu(buff, bit, 12, 1008); bit += 12;
  rtcm3_encode_1007_base(&rtcm_msg_1008->msg_1007, buff, &bit );
  setbitu(buff, bit, 8, rtcm_msg_1008->serial_count); bit += 8;
  for( u8 i =0; i < rtcm_msg_1008->serial_count; ++i ) {
    setbitu(buff, bit, 8, rtcm_msg_1008->serial_num[i]); bit += 8;
  }

  /* Round number of bits up to nearest whole byte. */
  return (bit + 7) / 8;
}

s8 rtcm3_decode_1001(const u8 *buff, rtcm_obs_message *rtcm_msg_1001 )
{
  rtcm3_read_header(buff, &rtcm_msg_1001->header);

  if (rtcm_msg_1001->header.msg_num != 1001)
    /* Unexpected message type. */
    return -1;

  /* TODO: Fill in t->wn. */

  u16 bit = 64;
  for (u8 i=0; i<rtcm_msg_1001->header.n_sat; i++) {
    init_data( &rtcm_msg_1001->sats[i] );

    /* TODO: Handle SBAS prns properly, numbered differently in RTCM? */
    rtcm_msg_1001->sats[i].svId = getbitu(buff, bit, 6); bit += 6;

    rtcm_freq_data *l1_freq_data = &rtcm_msg_1001->sats[i].obs[L1_FREQ];

    u32 l1_pr;
    s32 phr_pr_diff;
    s8 rtn = decode_basic_freq_data( buff, &bit, l1_freq_data, &l1_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    l1_freq_data->pseudorange = 0.02*l1_pr;
    l1_freq_data->flags.valid_pr = 1;
    l1_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L1_FREQ]);
    l1_freq_data->flags.valid_cp = 1;
  }

  return 0;
}

/** Decode an RTCMv3 message type 1002 (Extended L1-Only GPS RTK Observables)
 *
 * \param buff A pointer to the RTCM data message buffer.
 * \param id Reference station ID (DF003).
 * \param tow GPS time of week of epoch (DF004).
 * \param n_sat Number of GPS satellites included in the message (DF006).
 * \param nm Struct containing the observation.
 * \param sync Synchronous GNSS Flag (DF005).
 * \return If valid then return 0.
 *         Returns a negative number if the message is invalid:
 *          - `-1` : Message type mismatch
 *          - `-2` : Message uses unsupported P(Y) code
 */
s8 rtcm3_decode_1002(const u8 *buff, rtcm_obs_message *rtcm_msg_1002 )
{
  rtcm3_read_header(buff, &rtcm_msg_1002->header);

  if (rtcm_msg_1002->header.msg_num != 1002)
    /* Unexpected message type. */
    return -1;

  /* TODO: Fill in t->wn. */

  u16 bit = 64;
  for (u8 i=0; i<rtcm_msg_1002->header.n_sat; i++) {
    init_data( &rtcm_msg_1002->sats[i] );

    /* TODO: Handle SBAS prns properly, numbered differently in RTCM? */
    rtcm_msg_1002->sats[i].svId = getbitu(buff, bit, 6); bit += 6;

    rtcm_freq_data *l1_freq_data = &rtcm_msg_1002->sats[i].obs[L1_FREQ];

    u32 l1_pr;
    s32 phr_pr_diff;
    s8 rtn = decode_basic_freq_data( buff, &bit, l1_freq_data, &l1_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    u8 amb = getbitu(buff, bit, 8); bit += 8;
    l1_freq_data->cnr = 0.25*getbitu(buff, bit, 8); bit += 8;
    l1_freq_data->flags.valid_cnr = 1;

    l1_freq_data->pseudorange = 0.02*l1_pr + PRUNIT_GPS*amb;
    l1_freq_data->flags.valid_pr = 1;
    l1_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L1_FREQ]);
    l1_freq_data->flags.valid_cp = 1;
  }

  return 0;
}

s8 rtcm3_decode_1003(const u8 *buff, rtcm_obs_message *rtcm_msg_1003 )
{
  rtcm3_read_header(buff, &rtcm_msg_1003->header);

  if (rtcm_msg_1003->header.msg_num != 1003)
    /* Unexpected message type. */
    return -1;

  /* TODO: Fill in t->wn. */

  u16 bit = 64;
  for (u8 i=0; i<rtcm_msg_1003->header.n_sat; i++) {
    init_data( &rtcm_msg_1003->sats[i] );

    /* TODO: Handle SBAS prns properly, numbered differently in RTCM? */
    rtcm_msg_1003->sats[i].svId = getbitu(buff, bit, 6); bit += 6;

    rtcm_freq_data *l1_freq_data = &rtcm_msg_1003->sats[i].obs[L1_FREQ];

    u32 l1_pr, l2_pr;
    s32 phr_pr_diff;
    s8 rtn = decode_basic_freq_data( buff, &bit, l1_freq_data, &l1_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    l1_freq_data->pseudorange = 0.02*l1_pr;
    l1_freq_data->flags.valid_pr = 1;
    l1_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L1_FREQ]);
    l1_freq_data->flags.valid_cp = 1;

    rtcm_freq_data *l2_freq_data = &rtcm_msg_1003->sats[i].obs[L2_FREQ];

    rtn = decode_basic_freq_data( buff, &bit, l2_freq_data, &l2_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    l2_freq_data->pseudorange = 0.02*l2_pr + l1_freq_data->pseudorange;
    l2_freq_data->flags.valid_pr = 1;
    l2_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L2_FREQ]);
    l2_freq_data->flags.valid_cp = 1;
  }

  return 0;
}

s8 rtcm3_decode_1004(const u8 *buff, rtcm_obs_message *rtcm_msg_1004 )
{
  rtcm3_read_header(buff, &rtcm_msg_1004->header);

  if (rtcm_msg_1004->header.msg_num != 1004)
    /* Unexpected message type. */
    return -1;

  /* TODO: Fill in t->wn. */

  u16 bit = 64;
  for (u8 i=0; i<rtcm_msg_1004->header.n_sat; i++) {
    init_data( &rtcm_msg_1004->sats[i] );

    /* TODO: Handle SBAS prns properly, numbered differently in RTCM? */
    rtcm_msg_1004->sats[i].svId = getbitu(buff, bit, 6); bit += 6;

    rtcm_freq_data *l1_freq_data = &rtcm_msg_1004->sats[i].obs[L1_FREQ];

    u32 l1_pr, l2_pr;
    s32 phr_pr_diff;
    s8 rtn = decode_basic_freq_data( buff, &bit, l1_freq_data, &l1_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    u8 amb = getbitu(buff, bit, 8); bit += 8;
    l1_freq_data->cnr = 0.25*getbitu(buff, bit, 8); bit += 8;
    l1_freq_data->flags.valid_cnr = 1;

    l1_freq_data->pseudorange = 0.02*l1_pr + PRUNIT_GPS*amb;
    l1_freq_data->flags.valid_pr = 1;
    l1_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L1_FREQ]);
    l1_freq_data->flags.valid_cp = 1;

    rtcm_freq_data *l2_freq_data = &rtcm_msg_1004->sats[i].obs[L2_FREQ];

    rtn = decode_basic_freq_data( buff, &bit, l2_freq_data, &l2_pr, &phr_pr_diff );
    if (rtn)
      /* P(Y) code not currently supported. */
      return rtn;

    l2_freq_data->cnr = 0.25*getbitu(buff, bit, 8); bit += 8;
    l2_freq_data->flags.valid_cnr = 1;

    l2_freq_data->pseudorange = 0.02*l2_pr + l1_freq_data->pseudorange;
    l2_freq_data->flags.valid_pr = 1;
    l2_freq_data->carrier_phase = (l1_freq_data->pseudorange + 0.0005*phr_pr_diff) / (CLIGHT / FREQS[L2_FREQ]);
    l2_freq_data->flags.valid_cp = 1;
  }

  return 0;
}

s8 rtcm3_decode_1005_base(const u8 *buff, rtcm_msg_1005 *rtcm_msg_1005, u16 *bit ) {
  rtcm_msg_1005->stn_id = getbitu(buff, *bit, 12); *bit += 12;
  rtcm_msg_1005->ITRF = getbitu(buff, *bit, 6); *bit += 6;
  rtcm_msg_1005->GPS_ind = getbitu(buff, *bit, 1); *bit += 1;
  rtcm_msg_1005->GLO_ind = getbitu(buff, *bit, 1); *bit += 1;
  rtcm_msg_1005->GAL_ind = getbitu(buff, *bit, 1); *bit += 1;
  rtcm_msg_1005->ref_stn_ind = getbitu(buff, *bit, 1); *bit += 1;
  rtcm_msg_1005->arp_x =  (double)( getbitsl(buff, *bit, 38) ) / 10000.0; *bit += 38;
  rtcm_msg_1005->osc_ind = getbitu(buff, *bit, 1); *bit += 1;
  getbitu(buff, *bit, 1); *bit += 1;
  rtcm_msg_1005->arp_y =  (double)( getbitsl(buff, *bit, 38) ) / 10000.0; *bit += 38;
  rtcm_msg_1005->quart_cycle_ind = getbitu(buff, *bit, 2); *bit += 2;
  rtcm_msg_1005->arp_z =  (double)( getbitsl(buff, *bit, 38) ) / 10000.0; *bit += 38;

  return 0;
}

s8 rtcm3_decode_1005(const u8 *buff, rtcm_msg_1005 *rtcm_msg_1005 ) {
  u16 bit = 0;
  u16 msg_num = getbitu(buff, bit, 12); bit += 12;

  if (msg_num != 1005)
    /* Unexpected message type. */
    return -1;

  return rtcm3_decode_1005_base( buff, rtcm_msg_1005, &bit );
}

s8 rtcm3_decode_1006(const u8 *buff, rtcm_msg_1006 *rtcm_msg_1006 ) {
  u16 bit = 0;
  u16 msg_num = getbitu(buff, bit, 12); bit += 12;

  if (msg_num != 1006)
    /* Unexpected message type. */
    return -1;

  rtcm3_decode_1005_base( buff, &rtcm_msg_1006->msg_1005, &bit );
  rtcm_msg_1006->ant_height =  (double)( getbitu(buff, bit, 16) ) / 10000.0; bit += 16;
  return 0;
}

s8 rtcm3_decode_1007_base(const u8 *buff, rtcm_msg_1007 *rtcm_msg_1007, u16 *bit ) {
  rtcm_msg_1007->stn_id = getbitu(buff, *bit, 12); *bit += 12;
  rtcm_msg_1007->desc_count = getbitu(buff, *bit, 8); *bit += 8;
  for( u8 i = 0; i < rtcm_msg_1007->desc_count; ++i ) {
    rtcm_msg_1007->desc[i] = getbitu(buff, *bit, 8); *bit += 8;
  }
  rtcm_msg_1007->ant_id = getbitu(buff, *bit, 8); *bit += 8;

  return 0;
}

s8 rtcm3_decode_1007(const u8 *buff, rtcm_msg_1007 *rtcm_msg_1007 ) {
  u16 bit = 0;
  u16 msg_num = getbitu(buff, bit, 12); bit += 12;

  if (msg_num != 1007)
    /* Unexpected message type. */
    return -1;

  rtcm3_decode_1007_base( buff, rtcm_msg_1007, &bit );

  return 0;
}

s8 rtcm3_decode_1008(const u8 *buff, rtcm_msg_1008 *rtcm_msg_1008 ) {
  u16 bit = 0;
  u16 msg_num = getbitu(buff, bit, 12); bit += 12;

  if (msg_num != 1008)
    /* Unexpected message type. */
    return -1;

  rtcm3_decode_1007_base( buff, &rtcm_msg_1008->msg_1007, &bit );
  rtcm_msg_1008->serial_count = getbitu(buff, bit, 8); bit += 8;
  for( u8 i = 0; i < rtcm_msg_1008->serial_count; ++i ) {
    rtcm_msg_1008->serial_num[i] = getbitu(buff, bit, 8); bit += 8;
  }
  return 0;
}

s8 encode_basic_freq_data( const rtcm_freq_data *freq_data, freq_enum freq, const double *l1_pr, u8 *buff, u16 *bit ) {

  /* Calculate GPS Integer L1 Pseudorange Modulus Ambiguity (DF014). */
  u8 amb =  (u8)(*l1_pr / PRUNIT_GPS);

  /* Construct L1 pseudorange value as it would be transmitted (DF011). */
  u32 calc_l1_pr =  (u32)roundl((double)(*l1_pr - amb * PRUNIT_GPS) / 0.02);

  /* Calculate GPS Pseudorange (DF011/DF016). */
  u32 pr =  (u32)roundl((freq_data->pseudorange - amb * PRUNIT_GPS) / 0.02);
  if( freq != L1_FREQ ) {
      pr -= calc_l1_pr;
  }

  double l1_prc = calc_l1_pr * 0.02 + amb * PRUNIT_GPS;

  /* phaserange - L1 pseudorange */
  double cp_pr = freq_data->carrier_phase - l1_prc / (CLIGHT / FREQS[freq]);

// pgrgich: TO DO!!
//  /* If the phaserange and pseudorange have diverged close to the limits of the
//   * data field (20 bits) then we modify the carrier phase by an integer amount
//   * to bring it back into range an reset the phase lock time to zero to reset
//   * the integer ambiguity.
//   * The spec suggests adjusting by 1500 cycles but I calculate the range to be
//   * +/- 1379 cycles. Limit to just 1000 as that should still be plenty. */
//  if (fabs(cp_pr) > 1000) {
//    nm->lock_time = 0;
//    nm->raw_carrier_phase -= (s32)cp_pr;
//    cp_pr -= (s32)cp_pr;
//  }

  /* Calculate GPS PhaseRange – L1 Pseudorange (DF012/DF018). */
  s32 ppr = roundl(cp_pr * (CLIGHT / FREQS[freq]) / 0.0005);

  /* TODO: set GPS code indicator if we ever support P(Y) code measurements. */
  setbitu(buff, *bit, 1,  0);    *bit += 1;
  setbitu(buff, *bit, 24, pr);   *bit += 24;
  setbits(buff, *bit, 20, ppr);  *bit += 20;
  setbitu(buff, *bit, 7,  freq_data->flags.valid_lock ? to_lock_ind(freq_data->lock) : 0); *bit += 7;
}

s8 decode_basic_freq_data( const u8 *buff, u16 *bit, rtcm_freq_data *freq_data, u32 *pr, s32 *phr_pr_diff ) {
  freq_data->code = getbitu(buff, *bit, 1); *bit += 1;
  /* TODO: When we start storing the signal/system etc. properly we can
   * store the code flag in the nav meas struct. */
  if (freq_data->code == 1)
    /* P(Y) code not currently supported. */
    return -2;

  *pr = getbitu(buff, *bit,24); *bit += 24;
  *phr_pr_diff = getbits(buff, *bit,20); *bit += 20;

  freq_data->lock = from_lock_ind(getbitu(buff, *bit, 7)); *bit += 7;
  freq_data->flags.valid_lock = 1;

  return 0;
}

void init_data( rtcm_sat_data *sat_data ) {
  for( u8 freq = 0; freq < NUM_FREQS; ++freq ) {
    sat_data->obs[freq].flags.data = 0;
  }
}