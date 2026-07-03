Incluir en un archivo comprimido el código fuente, presentación e informe ( el mismo debe incluir el contexto completo de la solución, ventajas y limitaciones de la tecnología utilizada y conclusiones . Max 10 carillas). Para facilitar la corrección hacer una entrega por grupo, aclarando los apellidos en el nombre del archivo

Pautas Trabajo Integrador Final - IOT 2026 
Preferentemente deberá trabajarse en grupos de 2 personas.  
Fecha de entrega y presentación :  Miércoles 8/7 y Miércoles 15/7.  
Además de la entrega, cada grupo deberá exponer una  breve presentación de 10 minutos 
con la idea del proyecto llevada a cabo. 
Requisitos generales: 
● Todos los proyectos deben documentar el diseño, justificación técnica, y resultados. 
● En caso de no contar con sensores específicos, se permite la simulación mediante 
entradas analógicas, pulsadores o funciones en el código. 
● ENTREGABLES: 
○ Presentación en clase, donde se mencionan las particularidades del 
proyecto, alcance y solución. 
○ Códigos utilizados. 
○ Otra documentación complementaria del proyecto ( links, antecedentes, 
referencias a proyectos similares, herramientas utilizadas, etc.) 



Propuesta de Trabajo Integrador Final - IoT 2026

Sistema de geolocalización y monitoreo de ganado mediante red LoRa de bajo costo

Integrantes

Daskus, Bautista   -   Muñoz, Juan Pablo   -   Franco, Valentín

Definición del trabajo

En regiones ganaderas extensas (como la Patagonia) el ganado vacuno y ovino se desplaza por campos amplios sin cobertura celular, lo que dificulta su localización y el control de su bienestar. Los collares GPS comerciales dependen de redes móviles o satelitales y resultan costosos. Se propone desarrollar un sistema IoT de bajo costo y bajo consumo para rastrear y monitorear animales de ganado, basado en dos nodos ESP32 comunicados por LoRa (largo alcance, hasta varios km en línea de vista) e integrado a una plataforma de telemetría y visualización con las herramientas vistas en clase (MQTT, InfluxDB y Grafana).

Tomamos como referencia el ejemplo de 'Tracking - Geofencing' que propone la guía de trabajos de la materia (GPS + ESP32 que envía coordenadas periódicas, emite una alerta al salir de una zona predefinida y mapea las posiciones en Grafana). Nuestra propuesta retoma esa idea base y la extiende a un caso de uso ganadero, sumando un enlace LoRa de largo alcance entre dos nodos y el monitoreo de variables del bienestar animal (temperatura y actividad), de modo de cubrir zonas sin cobertura celular.

Alcance

Nodo collar (ESP32 Heltec LoRa): captura periódica de posición (GPS), temperatura corporal y aceleración/movimiento (acelerómetro), empaqueta las mediciones y las transmite por LoRa. Implementa alimentación autónoma por batería.

Nodo base/gateway (segundo ESP32 + módulo LoRa): recibe los paquetes del collar, los decodifica y los reenvía por Wi-Fi hacia un broker MQTT. Los datos se almacenan en InfluxDB y se visualizan en un tablero Grafana con el mapa de posiciones y el historial de recorrido.

Funcionalidades de aplicación: geocerca virtual (geofencing) con alerta al salir de la zona predefinida; alertas por temperatura elevada o inactividad prolongada; registro histórico de telemetría y mapeo del recorrido en Grafana.

Hardware disponible: ESP32 Heltec con LoRa, un módulo LoRa adicional, sensor de temperatura, GPS y acelerómetro. Resta incorporar un segundo ESP32 para la base y una batería de capacidad adecuada.


Comentario profe: Está ok el alcance, ya lo charlamos en clase. Recuerden diferenciar para este caso Lorawan ( que debería utilizar un Gateway y la infra de Lorawan)  versus un escenario p2p como el planteado

Hardware a utilizar: 
ESP32 (placa Heltec WiFi LoRa 32 V3)
módulo GPS GY-NEO6MV2
acelerometro MPU-6050
sensor de temperatura DS18B20
Estacion base: LoRa32u4(868-915MHz) (hay ejemplos de codigo y pinout en la carpeta de Tp final)

Indicaciones profe para la placa de estacion base: Para que la reconozca el IDE debemos copiar dentro la carpeta Arduino (Generalmente ubicada en Documentos/Arduino) una carpeta llamada hardware (si no existe crearla) y dentro de ella descomprimir el contenido del archivo bsfrance.rar . Luego al momento de compilar deben elegir la placa BsFrance->LoRa32u4(868-915MHz)

