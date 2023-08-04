inline uint32_t max5716_data_word(uint16_t data) {
    return (0b01 << 30) | (data << 14);
}