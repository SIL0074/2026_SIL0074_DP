def hex_to_int(h):
    res = 0
    for char in h.elems():
        res = res * 16
        c = char.lower()
        if c >= "0" and c <= "9":
            res += int(c)
        elif c >= "a" and c <= "f":
            res += ord(c) - ord("a") + 10
    return res

def power(base, exp):
    res = 1.0
    if exp > 0:
        for _ in range(exp):
            res *= base
    elif exp < 0:
        for _ in range(-exp):
            res /= base
    return res

def bytes_to_float(b):
    if len(b) != 4:
        return 0.0
    n = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)
    sign = -1.0 if (n >> 31) & 1 else 1.0
    exponent = (n >> 23) & 0xff
    fraction = n & 0x7fffff
    if exponent == 0: return 0.0
    if exponent == 255: return 0.0
    return sign * (1.0 + float(fraction) / 8388608.0) * power(2.0, exponent - 127)

def apply(metric):
    hex_str = metric.fields.get("data")
    if type(hex_str) != "string":
        return metric

    length = len(hex_str)
    if length != 16 and length != 32:
        return metric

    bytes_list = []
    for i in range(0, length, 2):
        bytes_list.append(hex_to_int(hex_str[i:i+2]))

    # Minimálně 2 floaty (Teplota, Vlhkost) - 8 bajtů = 16 hex znaků
    if len(bytes_list) >= 8:
        metric.fields["temperature_c"] = bytes_to_float(bytes_list[0:4])
        metric.fields["humidity_rh"] = bytes_to_float(bytes_list[4:8])

    # Další 2 floaty (Tlak, Plyn) - celkem 16 bajtů = 32 hex znaků
    if len(bytes_list) >= 16:
        metric.fields["pressure_hpa"] = bytes_to_float(bytes_list[8:12])
        metric.fields["gas_kohm"] = bytes_to_float(bytes_list[12:16])

    return metric
