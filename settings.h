#define SERIAL_LED 25
#define FRAME_PIN 8
#define MAX_PTS 4096

#define SERIAL_RX 13
#define SERIAL_TX 12

int DRAW_SPEED = 2;
int JUMP_SPEED = 10;

void init_settings()
{
    Serial1.setRX(SERIAL_RX);
    Serial1.setTX(SERIAL_TX);
    Serial1.begin(115200);
}
void update_settings()
{
    while (Serial1.available() > 0) {
        // read 2 ascii integers from serial
        int id = Serial1.parseInt();
        int val = Serial1.parseInt();

        // flush out until newline
        while (Serial1.read() != '\n')
            ;
        switch (id) {
        case 0:
            if (val > 0 && val < 4096) {
                DRAW_SPEED = val;
                Serial1.println("Speed updated to " + String(val));
            }
            break;
        case 1:
            if (val > 0) {
                JUMP_SPEED = val;
                Serial1.println("Jump speed updated to " + String(val));
            }
        }
    }
}