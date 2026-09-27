
import time
import json
import sys
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("La librería paho-mqtt no está instalada.")
    sys.exit(1)

BROKER_HOST = "localhost"
BROKER_PORT = 1883
TOPIC_BASE = "iot/a6f8b4147b"
TOPIC_COMMAND = f"{TOPIC_BASE}/command"
TOPIC_TELEMETRY = f"{TOPIC_BASE}/telemetry"
TOPIC_ALERT = f"{TOPIC_BASE}/alert"

def ejecutar_secuencia_pruebas():
    try:
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id="Python_Tester")
        except AttributeError:
            client = mqtt.Client(client_id="Python_Tester")

        print(f"Conectando a Mosquitto en {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        client.loop_start()

        time.sleep(1)

        print("\n--- PASO 1: Probar comando 'ABRIR' ---")
        client.publish(TOPIC_COMMAND, "ABRIR")
        print(f"-> Publicado en '{TOPIC_COMMAND}': ABRIR")
        time.sleep(2)

        print("\n--- PASO 2: Probar comando 'CERRAR' ---")
        client.publish(TOPIC_COMMAND, "CERRAR")
        print(f"-> Publicado en '{TOPIC_COMMAND}': CERRAR")
        time.sleep(2)

        print("\n--- PASO 3: Probar comando 'AUTO' ---")
        client.publish(TOPIC_COMMAND, "AUTO")
        print(f"-> Publicado en '{TOPIC_COMMAND}': AUTO")
        time.sleep(2)

        print("\n--- PASO 4: Probar Condición de Referencia: 'COMANDO_DESCONOCIDO' ---")
        client.publish(TOPIC_COMMAND, "COMANDO_DESCONOCIDO")
        print(f"-> Publicado en '{TOPIC_COMMAND}': COMANDO_DESCONOCIDO")
        print("   (El ESP32 debe responder rechazando el mensaje sin mover el actuador ni reiniciar)")
        time.sleep(3)

        print("\n--- PASO 5: Probar envío de comando inválido en formato JSON ---")
        cmd_invalido = json.dumps({"command": "INVALIDO_123"})
        client.publish(TOPIC_COMMAND, cmd_invalido)
        print(f"-> Publicado en '{TOPIC_COMMAND}': {cmd_invalido}")
        time.sleep(3)

        print("\n[ÉXITO] Secuencia de pruebas de comandos finalizada.")
        client.disconnect()

    except Exception as e:
        print(f"[ERROR] {e}")

if __name__ == "__main__":
    ejecutar_secuencia_pruebas()
