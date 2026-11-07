<div align="center">

# 🔥 Patata caliente con sockets

**Servidor y cliente en C que juegan a un concurso de preguntas sobre un protocolo propio, por TCP y por UDP**

[![C](https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=black)]()
[![TCP](https://img.shields.io/badge/TCP-conexi%C3%B3n-1F7A8C?style=flat)]()
[![UDP](https://img.shields.io/badge/UDP-datagramas-0F172A?style=flat)]()
[![Linux](https://img.shields.io/badge/Linux-sockets_BSD-FCC624?style=flat&logo=linux&logoColor=black)]()

[Descripción](#-sobre-el-proyecto) · [El juego](#-el-juego) · [Protocolo](#-el-protocolo) · [Cómo ejecutarlo](#-cómo-ejecutarlo) · [Estructura](#-estructura-del-repositorio) · [Stack](#-stack-tecnológico)

</div>

---

## 📖 Sobre el proyecto

Un **servidor** que atiende a varios clientes a la vez, **por TCP y por UDP**, y un **cliente** que se comunica con él mediante un protocolo de texto de inspiración SMTP (mensajes con código numérico, terminados en `\r\n`). Sobre ese protocolo se juega a una versión de preguntas y respuestas del juego de la **patata caliente**.

La práctica sirve para tocar de cerca las **diferencias entre TCP y UDP**: con TCP hay conexión, orden y fiabilidad; con UDP, cada mensaje es un datagrama independiente y hay que gestionar más cosas a mano.

> 📌 **Proyecto académico** de la asignatura de *Redes I* (curso 2023-2024, Universidad de Salamanca), en pareja con Jorge Caballero González.

---

## 🎲 El juego

1. El cliente saluda al servidor.
2. El servidor elige una pregunta de [`preguntas.txt`](preguntas.txt) (cuya respuesta es un número) y un número de **intentos al azar entre 2 y 6**.
3. El cliente envía su respuesta. El servidor contesta si el número real es **mayor** o **menor** y descuenta un intento.
4. Si acierta, **gana**; si se queda sin intentos, **pierde la patata**. Con `+` pide otra pregunta y con `ADIOS` termina.

Las preguntas tienen el formato `pregunta*respuesta`:

```text
¿Cuántos estados tiene Estados Unidos?*51
¿En qué año se descubrió América?*1492
```

---

## 📡 El protocolo

El cliente envía órdenes de texto: un saludo (`HELO` / `HOLA`), `RESPUESTA <n>`, `+` para pedir otra pregunta y `ADIOS`. El servidor contesta siempre con un código:

| Código | Significado |
| :--- | :--- |
| `220` | Bienvenida al abrir una conexión TCP. |
| `250` | Saludo aceptado, o nueva pregunta: `250 <pregunta>#<intentos>`. |
| `354` | La respuesta es incorrecta: `MENOR` o `MAYOR`, con los intentos que quedan (`#<n>`). |
| `350` | `ACIERTO`. |
| `375` | `FALLO`: no quedan más intentos. |
| `221` | `ADIOS`: se cierra el servicio. |
| `500` | `ERROR DE SINTAXIS`: el mensaje no tiene el formato esperado (por ejemplo, `RESPUESTA` sin un número). |

Las órdenes de ejemplo del cliente están en `ordenes.txt`, `ordenes1.txt` y `ordenes2.txt`.

**Cómo atiende el servidor:** escucha a la vez en un socket TCP y en uno UDP con `select()`. Con TCP crea **un proceso hijo y un socket por cada cliente**; con UDP responde a cada datagrama directamente. Cada conexión deja su registro en `peticionesTCP.log` / `peticionesUDP.log` y cada cliente, un fichero con el número de su puerto.

---

## 🚀 Cómo ejecutarlo

**Requisitos:** Linux (o WSL), `gcc` y `make`.

```bash
make                       # compila servidor y cliente
./servidor                 # arranca el servidor (puerto 1921)

# en otra terminal: cliente <host> <TCP|UDP> <fichero de órdenes>
./cliente localhost TCP ordenes.txt
./cliente localhost UDP ordenes1.txt
```

`lanzaServidor.sh` arranca el servidor y seis clientes a la vez (tres TCP y tres UDP). Está escrito para un equipo concreto (`nogal`): cambia ese nombre por `localhost` para usarlo en tu máquina.

---

## 📂 Estructura del repositorio

```
Sockets-Hot-Potato/
├── servidor.c         # Servidor concurrente TCP + UDP
├── cliente.c          # Cliente que ejecuta un fichero de órdenes
├── makefile
├── lanzaServidor.sh   # Lanza el servidor y varios clientes
├── preguntas.txt      # Banco de preguntas (pregunta*respuesta)
├── ordenes*.txt       # Órdenes de ejemplo para el cliente
└── docs/Informe-Sockets.pdf   # Informe de la práctica (con los datos personales tapados)
```

---

## 🖥️ Stack tecnológico

| Categoría | Herramientas |
| :--- | :--- |
| **Lenguaje** | C |
| **Redes** | Sockets BSD · TCP · UDP · `select()` · `fork()` |
| **Protocolo** | Mensajes de texto con código numérico, estilo SMTP |
| **Compilación** | `gcc` · `make` |

---

## 🎓 Qué aprendí

- Las **diferencias reales entre TCP y UDP**, más allá de la teoría.
- A **atender a varios clientes a la vez** con procesos hijos y con multiplexación (`select`).
- A **diseñar un protocolo de aplicación** y validar cada mensaje que llega.

---

## 👤 Autoría

**Unai García** y **Jorge Caballero González** — Grado en Ingeniería Informática (Universidad de Salamanca)

- 🐙 GitHub: [@unaigb](https://github.com/unaigb)

---

<div align="center">

**⭐ Si este repositorio te resulta útil, considera darle una estrella.**

</div>
