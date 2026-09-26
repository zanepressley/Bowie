#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// Wi-Fi
const char* SSID = "Pixel_4677";
const char* PASSWORD = "heyheyhey";
const uint16_t UDP_PORT = 4210;

// Motor pins
const int LEFT_PWM_PIN = 18;
const int RIGHT_PWM_PIN = 19;
const int RIGHT_DIR_1_PIN = 1;
const int RIGHT_DIR_2_PIN = 1;
const int LEFT_DIR_1_PIN = 1;
const int LEFT_DIR_2_PIN = 1;
const int SERVO_PIN = 1;

// Safety timeout
const uint32_t COMMAND_TIMEOUT_MS = 500;

// Command sent over Wi-Fi
struct TeleopCommand {
    float left_drive;
    float right_drive;
    float servo_pos;
    uint32_t timestamp;
};

QueueHandle_t commandQueue;


// Tasks
void networkTask(void* parameter);
void motorTask(void* parameter);


// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println("     SUBMARINE CONTROLLER");
    Serial.println("================================");

    pinMode(LEFT_PWM_PIN, OUTPUT);
    pinMode(RIGHT_PWM_PIN, OUTPUT);

    analogWrite(LEFT_PWM_PIN, 0);
    analogWrite(RIGHT_PWM_PIN, 0);

    commandQueue = xQueueCreate(1, sizeof(TeleopCommand));

    if (commandQueue == NULL)
    {
        Serial.println("ERROR: Queue creation failed!");
        while (true);
    }

    Serial.println("Command queue: OK");

    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);


Serial.println("Scanning for Wi-Fi...");

int networks = WiFi.scanNetworks();

for (int i = 0; i < networks; i++)
{
    Serial.print(i);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI(i));
    Serial.print("  Channel: ");
    Serial.println(WiFi.channel(i));
}

    WiFi.begin(SSID, PASSWORD);

    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi: CONNECTED");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Start network task on Core 0
    xTaskCreatePinnedToCore(
        networkTask,
        "Network",
        4096,
        NULL,
        1,
        NULL,
        0
    );

    // Start motor task on Core 1
    xTaskCreatePinnedToCore(
        motorTask,
        "Motor",
        4096,
        NULL,
        2,
        NULL,
        1
    );

    Serial.println("Network task: Core 0");
    Serial.println("Motor task:   Core 1");
    Serial.println("System: READY");
    Serial.println();
}


// --------------------------------------------------
// Main loop
// --------------------------------------------------

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}


// --------------------------------------------------
// Network task - Core 0
// --------------------------------------------------

void networkTask(void* parameter)
{
    WiFiUDP udp;

    udp.begin(UDP_PORT);

    Serial.print("UDP listening on port ");
    Serial.println(UDP_PORT);

    TeleopCommand command;

    while (true)
    {
        int packetSize = udp.parsePacket();

        if (packetSize > 0)
        {
            if (packetSize == sizeof(TeleopCommand))
            {
                int bytes = udp.read(
                    (char*)&command,
                    sizeof(TeleopCommand)
                );

                if (bytes == sizeof(TeleopCommand))
                {
                    command.timestamp = millis();

                    command.left_drive = constrain(
                        command.left_drive,
                        -1.0f,
                        1.0f
                    );

                    command.right_drive = constrain(
                        command.right_drive,
                        -1.0f,
                        1.0f
                    );

                    xQueueOverwrite(
                        commandQueue,
                        &command
                    );

                    Serial.print("TELEOP RX | L: ");
                    Serial.print(command.left_drive, 2);

                    Serial.print(" | R: ");
                    Serial.print(command.right_drive, 2);

                    Serial.println(" | OK");
                }
            }
            else
            {
                Serial.print("UDP ERROR | Size: ");
                Serial.println(packetSize);

                udp.flush();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


// --------------------------------------------------
// Motor task - Core 1
// --------------------------------------------------

void motorTask(void* parameter)
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    const TickType_t period =
        pdMS_TO_TICKS(20);

    TeleopCommand command = {
        0.0f,
        0.0f,
        0.0f,
        0
    };

    uint32_t lastStatus = 0;

    while (true)
    {
        TeleopCommand newCommand;

        if (xQueueReceive(commandQueue, &newCommand, 0))
        {
            command = newCommand;
        }

        bool timeout =
            millis() - command.timestamp >
            COMMAND_TIMEOUT_MS;

        float left = 0.0f;
        float right = 0.0f;
        float servo_pos = 0.0f;

        if (!timeout)
        {
            left = command.left_drive;
            right = command.right_drive;
            servo_pos = command.servo_pos;
        }

        int leftPWM =
            (int)(fabs(left) * 255.0f);

        int rightPWM =
            (int)(fabs(right) * 255.0f);

        int servoPWM =
            (int)(fabs(servo_pos) * 255.0f);

        analogWrite(
            LEFT_PWM_PIN,
            leftPWM
        );

        analogWrite(
            RIGHT_PWM_PIN,
            rightPWM
        );

        analogWrite(
            SERVO_PIN,
            servoPWM
        );

        // Status every second
        if (millis() - lastStatus >= 1000)
        {
            lastStatus = millis();

            Serial.print("MOTOR | L: ");
            Serial.print(left, 2);

            Serial.print(" (");
            Serial.print(leftPWM);
            Serial.print(")");

            Serial.print(" | R: ");
            Serial.print(right, 2);

            Serial.print(" (");
            Serial.print(rightPWM);
            Serial.print(")");

            Serial.print("\n SERVO POSITION: ");
            Serial.print(servo_pos, 2);

            if (timeout)
            {
                Serial.println(" | TIMEOUT - STOPPED");
            }
            else
            {
                Serial.println(" | RUNNING");
            }
        }

        vTaskDelayUntil(
            &lastWakeTime,
            period
        );
    }
}
