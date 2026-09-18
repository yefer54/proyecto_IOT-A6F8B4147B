# Interpretación de la asignación

## ¿Qué situación se observa en el contexto asignado?

Se logra observar una situación que requiere de un mecanismo de autenticación que permita validar credenciales autorizadas y no autorizadas para el ingreso a una zona de estudio.

## ¿Quién podría usar la solución y para qué?

Podría ser utilizada por cualquier entidad o institución con zonas privadas que necesite de un identificador de credenciales para permitir el acceso solo a personal autorizado.

## ¿Cuál es la entrada, qué decisión deberá tomar posteriormente el ESP32 y cuál es la salida?

- **Entrada:** Es el lector RFID cuando recibe el UID de la credencial.

- **Decisión:** La toma el ESP32 cuando compara el UID de la credencial con los que están permitidos (`30 4F 8E C0`).

- **Salida:** La da el ESP32 permitiendo el acceso, denegando el acceso o bloqueando el acceso durante 13 segundos.

## ¿Qué significa la regla individual y qué riesgo reduce el estado seguro?

La regla individual le indica al ESP32 que solo permita el acceso a las credenciales con el UID permitido; de lo contrario, deniega el acceso.

El estado seguro reduce el riesgo de que credenciales sin acceso permitido puedan ingresar a la zona restringida.

## Dos supuestos y dos limitaciones del primer prototipo

### Supuestos

1. El lector lee de manera correcta el UID de cada credencial.
2. Existe una conexión adecuada entre todos los componentes.

### Limitaciones

1. Actualmente se trabaja con una simulación y no con componentes físicos reales.
2. Solo tiene registrada una credencial permitida, lo que le da acceso a una sola persona. Para aplicarlo a un caso real, se necesitaría registrar más credenciales permitidas.