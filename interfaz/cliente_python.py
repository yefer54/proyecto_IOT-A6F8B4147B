

import sys
import time
import json
import threading
try:
    import paho.mqtt.client as mqtt
except ImportError:
    print(" La librería paho-mqtt no está instalada. Instálala ejecutando: pip install paho-mqtt")
    sys.exit(1)

BROKER_HOST = "localhost"
BROKER_PORT = 1883
TOPIC_BASE = "iot/a6f8b4147b"
TOPIC_TELEMETRY = f"{TOPIC_BASE}/telemetry"
TOPIC_STATUS = f"{TOPIC_BASE}/status"
TOPIC_COMMAND = f"{TOPIC_BASE}/command"
TOPIC_ALERT = f"{TOPIC_BASE}/alert"

DEVICE_ID_ESPERADO = "IOT-A6F8B4147B"
VARIABLE_ESPERADA = "identificador RFID"
UNIDAD_ESPERADA = "segundos de bloqueo"

def validar_json_telemetria(data):

    errores = []
    campos_requeridos = ["device_id", "variable", "value", "unit", "mode", "alarm", "sequence"]
    
    for campo in campos_requeridos:
        if campo not in data:
            errores.append(f"Falta el campo '{campo}'")
            
    if "device_id" in data and not isinstance(data["device_id"], str):
        errores.append("'device_id' debe ser de tipo String")
    if "variable" in data and not isinstance(data["variable"], str):
        errores.append("'variable' debe ser de tipo String")
    if "value" in data and not isinstance(data["value"], (int, float)):
        errores.append("'value' debe ser de tipo Number/Float")
    if "unit" in data and not isinstance(data["unit"], str):
        errores.append("'unit' debe ser de tipo String")
    if "mode" in data and not isinstance(data["mode"], str):
        errores.append("'mode' debe ser de tipo String")
    if "alarm" in data and not isinstance(data["alarm"], bool):
        errores.append("'alarm' debe ser de tipo Boolean")
    if "sequence" in data and not isinstance(data["sequence"], int):
        errores.append("'sequence' debe ser de tipo Integer")
        
    return len(errores) == 0, errores

def on_connect(client, userdata, flags, rc, *args):
    if rc == 0:
        print(f"\n Conexión exitosa al broker Mosquitto ({BROKER_HOST}:{BROKER_PORT})")
        # Suscribirse a todos los subtemas de la asignación 3.4
        client.subscribe([(TOPIC_TELEMETRY, 0), (TOPIC_STATUS, 0), (TOPIC_ALERT, 0), (TOPIC_COMMAND, 0)])
        print(f" Escuchando en topics:")
        print(f"  - Telemetría: {TOPIC_TELEMETRY}")
        print(f"  - Estado:     {TOPIC_STATUS}")
        print(f"  - Alertas:    {TOPIC_ALERT}")
        print(f"  - Comandos:   {TOPIC_COMMAND}\n")
    else:
        print(f"ERROR CONEXIÓN - Código de retorno: {rc}")

def on_message(client, userdata, msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    topic = msg.topic
    payload_raw = msg.payload.decode('utf-8', errors='ignore')

    if topic == TOPIC_TELEMETRY:
        try:
            data = json.loads(payload_raw)
            valido, errores = validar_json_telemetria(data)
            status_tag = "VALIDO" if valido else f"INVALIDO ({', '.join(errores)})"
            print(f"\n[{timestamp}] [TELEMETRÍA #{data.get('sequence')}] [{status_tag}] {{")
            for k, v in data.items():
                if isinstance(v, str):
                    print(f'"{k}":"{v}"')
                elif isinstance(v, bool):
                    print(f'"{k}":{str(v).lower()}')
                else:
                    print(f'"{k}":{v}')
            print("}")
        except json.JSONDecodeError:
            print(f"\n[{timestamp}] [TELEMETRÍA INVALIDA] Payload no es JSON válido: {payload_raw}")

    elif topic == TOPIC_ALERT:
        print(f"\n[{timestamp}] [ALERTA INMEDIATA]")
        try:
            data = json.loads(payload_raw)
            print("MQTT ALERTA INMEDIATA {")
            for k, v in data.items():
                if isinstance(v, str):
                    print(f'"{k}":"{v}"')
                elif isinstance(v, bool):
                    print(f'"{k}":{str(v).lower()}')
                else:
                    print(f'"{k}":{v}')
            print("}")
        except json.JSONDecodeError:
            print(f"  Contenido: {payload_raw}")

    elif topic == TOPIC_STATUS:
        print(f"\n[{timestamp}] [ESTADO DISPOSITIVO]  Topic: {topic}")
        print(f"  Contenido: {payload_raw}")

    elif topic == TOPIC_COMMAND:
        print(f"\n[{timestamp}] [COMANDO PUBLICADO]  Topic: {topic}")
        print(f"  Comando: {payload_raw}")

def CLI_envio_comandos(client):
    time.sleep(1)
    while True:
        print(" 1. Enviar comando 'AUTO'    (Control automático por sensor)")
        print(" 2. Enviar comando 'ABRIR'   (Apertura manual - Servo a 90°)")
        print(" 3. Enviar comando 'CERRAR'  (Cierre manual - Servo a 0°)")
        print(" 4. Enviar 'COMANDO_DESCONOCIDO' (Prueba rechazo sin reinicio)")
        print(" 5. Enviar comando personalizado en texto o JSON")
        print(" 6. Salir")
        
        opcion = input("Seleccione una opción (1-6): ").strip()
        
        if opcion == "1":
            client.publish(TOPIC_COMMAND, "AUTO")
            print(f"[ENVIADO] -> '{TOPIC_COMMAND}': AUTO")
            print("  ℹ️ Explicación: Reactiva el control automático mediante lecturas del sensor RFID.")
        elif opcion == "2":
            client.publish(TOPIC_COMMAND, "ABRIR")
            print(f"[ENVIADO] -> '{TOPIC_COMMAND}': ABRIR")
            print("  ℹ️ Explicación: Abre manualmente la puerta situando el servomotor a 90°.")
        elif opcion == "3":
            client.publish(TOPIC_COMMAND, "CERRAR")
            print(f"[ENVIADO] -> '{TOPIC_COMMAND}': CERRAR")
            print("  ℹ️ Explicación: Cierra manualmente la puerta situando el servomotor a 0° (Estado Seguro).")
        elif opcion == "4":
            client.publish(TOPIC_COMMAND, "COMANDO_DESCONOCIDO")
            print(f"[ENVIADO] -> '{TOPIC_COMMAND}': COMANDO_DESCONOCIDO")
            print("  ℹ️ Explicación: Prueba el rechazo de órdenes inválidas sin alterar el actuador ni reiniciar el ESP32.")
        elif opcion == "5":
            custom_cmd = input("Ingrese el comando a enviar: ").strip()
            client.publish(TOPIC_COMMAND, custom_cmd)
            print(f"[ENVIADO] -> '{TOPIC_COMMAND}': {custom_cmd}")
            print("  ℹ️ Explicación: Envió una orden personalizada para evaluación del ESP32.")
        elif opcion == "6":
            print("[SALIR] Finalizando cliente Python...")
            client.disconnect()
            break
        else:
            print("[OPCIÓN INVÁLIDA] Intente de nuevo.")

def main():
    try:
        try:
            client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id="Python_Client_IOT")
        except AttributeError:
            client = mqtt.Client(client_id="Python_Client_IOT")

        client.on_connect = on_connect
        client.on_message = on_message

        print(f"[INICIANDO] Conectando al broker MQTT Mosquitto local {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)

        client.loop_start()

        CLI_envio_comandos(client)

    except KeyboardInterrupt:
        print("\n[SALIR] Interrupción por teclado.")
    except Exception as e:
        print(f"[ERROR EN EJECUCIÓN] {e}")

if __name__ == "__main__":
    main()
