# Interpretación de la asignación

## Cómo debe comportarse localmente tu sistema

El sistema funciona a través de un lector RFID que recibe los códigos de diferentes credenciales. Seguidamente, le envía la información al ESP32, el cual compara el código de la credencial registrada con el/los códigos que permiten el ingreso y los que no, para después indicarle al servomotor si debe permitir el acceso a la credencial ingresada o denegárselo.

## Cuándo se activa y cuándo regresa a la condición normal

Se activa en el momento en que el código de la credencial es el correcto. En ese momento, el servomotor se abre 90° y permite el ingreso. 1 segundo después, el servomotor se cierra y regresa a su estado normal.

## Qué función cumplen las confirmaciones y la histéresis asignadas

Se activa cuando ha llegado a los 3 intentos fallidos. En ese momento, el servomotor se bloquea durante 13 segundos y no permite el ingreso de ninguna credencial durante ese lapso de tiempo.

## Qué información se enviará a la nube

Por medio de MQTT se enviará a la nube la cantidad de registros autorizados y denegados, llevando un conteo de las credenciales que se recibieron durante un tiempo determinado.

## Dos riesgos o dificultades de este segundo avance

1. **Pérdida de conexión Wi-Fi:** podría impedir que la información viaje a la nube correctamente.

2. **Periodo de inactividad de los dispositivos:** el servomotor y el lector RFID tienen un periodo de tiempo de inactividad entre cada registro. Si se reciben muchos registros en muy poco tiempo, el sistema podría fallar.