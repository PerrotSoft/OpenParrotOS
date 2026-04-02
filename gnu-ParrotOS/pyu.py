import re

def convert_array(c_code):
    # Ищем имя массива и содержимое
    match = re.search(r'const\s+uint8_t\s+(\w+)\s*\[(\d+)\]\s*=\s*{([^}]*)}', c_code, re.S)
    if not match:
        raise ValueError("Не удалось распарсить массив")

    name = match.group(1)
    size = int(match.group(2))
    values_str = match.group(3)

    # Парсим значения
    values = [int(v.strip(), 0) for v in values_str.split(',') if v.strip()]

    # Определяем ширину (предположим, 8 пикселей в строке)
    width = 8
    height = len(values) // width

    # Конвертируем в битовую форму
    result_bytes = []
    for row in range(height):
        byte = 0
        for col in range(width):
            byte = (byte << 1) | (1 if values[row*width + col] else 0)
        result_bytes.append(byte)

    # Формируем новый C-массив
    output = f"const uint8_t {name}[{height}] = {{\n"
    for b in result_bytes:
        output += f"    0b{b:08b},\n"
    output += "};\n"

    return output

# === Пример использования ===
c_code = """
const uint8_t bitmap_G[35] = {
    0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00,
};

"""

print(convert_array(c_code))
