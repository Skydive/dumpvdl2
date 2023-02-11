import numpy as np

from crccheck.crc import Crc16, CrcX25, Crc16Base
from bitarray import bitarray
from bitarray.util import ba2hex
from struct import pack
from binascii import hexlify

from collections import namedtuple


ADDRESS_TYPE = {"Aircraft ICAO24": bitarray('001'), # have address all ones for aircraft broadcast
                "Ground Station ICAO-administred address space": bitarray('100'), # provider code filled with all ones for provider broadcast or all ones for all ground stations
                "Ground Station ICAO-delegated address space": bitarray('101'), # see previous comment
                "All Stations broadcast": bitarray('001')} # all ones

FRAME_TYPE = {"Command": bitarray('0'),
              "Response": bitarray('1')}

AIR_GROUND_STATUS = {"Air": bitarray('0'),
                     "Ground": bitarray('1')}


def parity(message):
    m = list(message)
    for i in range(len(m)):
        if bin(m[i])[2:].count("1") % 2 == 0:
            m[i] = bytes([m[i] | 0x80])
        else:
            m[i] = bytes([m[i]])
    return b"".join(m)


def reverse_bits_bitstring(binary_message):
    result = [binary_message[7::-1]]
    for i in range(8, len(binary_message), 8):
        result.append(binary_message[i+7:i-1:-1])
    return "".join(result)


def reverse_bits(char_message):
    result = b""
    for d in char_message:
        e = (d >> 7 & 0x1) \
            | (d >> 5 & 0x2) \
            | (d >> 3 & 0x4) \
            | (d >> 1 & 0x8) \
            | (d << 1 & 0x10) \
            | (d << 3 & 0x20) \
            | (d << 5 & 0x40) \
            | (d << 7 & 0x80)
        result += bytes([e])
    return result


# It's done here: https://datatracker.ietf.org/doc/rfc1662/ (RFC 1662)
def calc_avlc_crc(msg):
    crc = CrcX25.calcbytes(msg)[::-1]
    return crc

def calc_acars_crc(msg):
    crc = Crc16.calcbytes(reverse_bits(msg))
    return reverse_bits(crc)

def build_acars_message(mode, registration, ack, label, block_id, msg_number, flight_number, msg_text):
    preamble = b"\xff" * 16  # preamble length of 16 symbols
    bit_synchronization = b"+*"
    character_synchronization = b"\x16\x16"  # SYN
    soh = b"\x01"
    heading = mode + registration + ack + label + block_id
    stx = b"\x02"
    text = msg_number + flight_number + msg_text
    etx = b"\x03"
    bcs_suffix = b"\x7f"
    bit_synchronization = parity(bit_synchronization)
    character_synchronization = parity(character_synchronization)
    heading = parity(heading)
    stx = parity(stx)
    text = parity(text)
    etx = parity(etx)
    bcs_suffix = parity(bcs_suffix)
    block_check_sequence = calc_acars_crc(heading + stx + text + etx)
    # block_check_sequence = calc_acars_crc(heading + text + etx)
    msg = preamble + \
          bit_synchronization + \
          character_synchronization + \
          soh + \
          heading + \
          stx + \
          text + \
          etx + \
          block_check_sequence + \
          bcs_suffix
    return msg


def build_avlc_frame(avlc_address, acars_message):
    # according to Draft ETSI EN 301 841-2 V1.1.1 (2003-07)
    # FLAG = bitarray('01111110')
    FLAG = b'\x7e'
    # destination c0ffee + 001 (aircraft) + 1 (ground-air) bytes reversed
    # destination_address = [bitarray('11000110'), bitarray('11111010'), bitarray('01111110'), bitarray('11000001')]
    # destination_address = b"\xc6\xfa\x7e\xc1"
    destination_address = avlc_address['dst'][:4];
    # print("TYPE: ", type(destination_address))
    # source address DEFACE + 100 (ground) + 0 (command frame) bytes reversed
    # source_address = [bitarray('11010010'), bitarray('1011001'), bitarray('01111100'), bitarray('11011111')]
    source_address = avlc_address['src'][:4];
    # link_control_field = bitarray('00000011')
    # link_control_field = b"\x02"
    link_control_field = b"\x03"
    information = acars_message

    block = destination_address + source_address + link_control_field + information;
    frame_check_sequence = calc_avlc_crc(block)
    # test_calculate_residue(block);
    # msg = FLAG + \
    msg = block + frame_check_sequence
    return msg


def avlc_interleaver(avlc_frame):
    # bit interleaving for byte; move first bit to last:
    # interleaved = byte[1:] + byte[:1]

    # slice frame into 249 byte chunks to prepare them for interleaving and fec
    number_of_chunks = np.ceil(len(avlc_frame) / 1992)
    sliced_frame = np.array_split((np.array(avlc_frame)), number_of_chunks)
    if len(sliced_frame[-1]):
        sliced_frame[-1] = np.pad(sliced_frame[-1], (0, 1992 - len(c)), mode="constant", constant_values=0)

    for i in range(len(sliced_frame)):
        row = sliced_frame[i]
        new_row = np.array([], dtype=bool)
        for octet in np.split(row, 8):
            new_row = np.concatenate((new_row, octet[1:], octet[:1]))
        sliced_frame[i] = new_row

    return bitarray(np.concatenate(sliced_frame).tolist())


def avlc_fec(avlc_frame):
    # slice into 249byte chunks for fec
    number_of_chunks = np.ceil(len(avlc_frame) / 1992)
    sliced_frame = np.array_split((np.array(avlc_frame)), number_of_chunks)

    for i in range(len(sliced_frame)):
        # static for now
        fec = np.array([0 for _ in range(16)], dtype=bool)
        sliced_frame = np.concatenate((sliced_frame, fec))

    return bitarray(np.concatenate(sliced_frame).tolist())


def avlc_scrambler():
    # poly: x^15+x+1
    # initial: 1101 0010 1011 001
    pass



def d8psk_modulate(signal, amplitude):
    diff_phase = {bitarray('000'): complex(amplitude),
                  bitarray('001'): complex(amplitude / np.sqrt(2), amplitude / np.sqrt(2)),
                  bitarray('011'): complex(0,amplitude),
                  bitarray('010'): complex(-1 * amplitude / np.sqrt(2), amplitude / np.sqrt(2)),
                  bitarray('110'): complex(-1 * amplitude),
                  bitarray('111'): complex(-1 * amplitude / np.sqrt(2), -1 * amplitude / np.sqrt(2)),
                  bitarray('101'): complex(0, -1 * amplitude),
                  bitarray('100'): complex(amplitude / np.sqrt(2), -1 * amplitude / np.sqrt(2))}
    return None


def vdlm2_header_fec(transmission_length):
    H = np.array([[0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
                  [0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1],
                  [1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1],
                  [1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1],
                  [0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1]], dtype=bool)
    a = np.array(transmission_length.extend([0,0,0]).reverse())
    return bitarray(np.dot(a, H.T).tolist()).reverse() # use reverse here so everything is msb to lsb until we reverse all of them




if __name__ == "__main__":
    sample_rate = 2.1e6
    baud_rate = 10.5e3
    samples_per_baud = sample_rate / baud_rate

    signal_strength = -20 # dBFS
    amplitude = 10 ** (signal_strength/20)

    # create "training sequence"
    ramp_up = ['000' for _ in range(5)]
    # 000 010 011 110 000 001 101 110 001 100 011 111 101 111 100 010
    unique_word = ['000', '010', '011', '110', '000', '001', '101', '110',
                '001', '100', '011', '111', '101', '111', '100', '010']
    reserved_symbol = '000'
    # transmission length after header and fec; length 17bit
    transmission_length = '00000000000000000'
    header_fec = '00000'

    msg = build_acars_message(b"2", b"..PWNED", b"\x15", b"Q5", b"0", b"M01A", b"COFFEE",
                            b"This is a spoofed ACARS message! Beware!")

    avlc_address = {
        "dst": b"\xc6\xfa\x7e\xc1",
        "src": b"\xD2\x59\x7C\xDF"
    };

    avlc_frame = build_avlc_frame(avlc_address, msg);

    import sys
    sys.stdout.buffer.write(avlc_frame)
    # rev_msg = reverse_bits(msg)

    # signal = d8psk_modulate(rev_msg)


