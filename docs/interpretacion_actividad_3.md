# Interpretación de la asignación

## ¿Cuál es el recorrido de una medición desde el sensor hasta la interfaz?

Cuando el sensor RFID detecta una tarjeta, el ESP32 lee la credencial y decide si el acceso es autorizado o no. Con ese resultado construye un JSON y lo publica en alguno de los *topics* hacia el broker Mosquitto.

Python está conectado a esos *topics*, recibe el mensaje y lo muestra en consola.

## ¿Cuál es el recorrido de un comando desde la interfaz hasta el actuador?

Python muestra un mensaje en consola como `ABRIR`, `CERRAR` o `AUTO`. Una vez hecha la elección, se envía la decisión al ESP32 para posteriormente indicarle al servomotor y al LED cómo actuar.

## ¿Qué decisiones deben permanecer en el ESP32 y por qué?

Las decisiones críticas quedan en el ESP32 porque es el único que tiene acceso directo al hardware.

La validación del UID autorizado, el conteo de rechazos, el bloqueo de 13 segundos y el control del servomotor no pueden depender de la red, porque si se cae la conexión, el sistema debe seguir funcionando de forma segura.

## ¿Qué función cumplen Python, los topics y el JSON asignado?

### Python

Python es la interfaz desde donde se monitorea el sistema y se envían comandos.

### Topics

Los *topics* organizan el tráfico. Cada tipo de mensaje va por su propio canal:

- `telemetry`
- `alert`
- `status`
- `command`

Esto permite que cada tipo de información sea recibido únicamente por los dispositivos o componentes que correspondan.

### JSON

El JSON es el formato de los datos que viajan entre los componentes del sistema. Al tener campos fijos como `device_id`, `value`, `mode` o `alarm`, es más fácil verificar que el mensaje llegó completo y sin errores.