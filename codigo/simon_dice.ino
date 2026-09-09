// ===== SIMON DICE - ESP32: 3 Modos x 3 Dificultades + Carrusel + Pagina Web (AP) =====
#include <Keypad.h>
#include <WiFi.h>
#include <WebServer.h>

// ---------- LEDs ----------
const int NUM_COLORES = 3;
const int pinesLED[NUM_COLORES] = { 25, 26, 33 };  // Verde, Amarillo, Rojo

// ---------- Botones Simon (Modo 1) ----------
const int pinesBoton[NUM_COLORES] = { 2, 15, 23 };  // Verde, Amarillo, Rojo (pines con pull-up interno)

// ---------- Buzzer ----------
const int pinBuzzer = 27;
const int tonos[NUM_COLORES] = { 392, 330, 262 };  // Sol, Mi, Do (Verde, Amarillo, Rojo)

// ---------- Joystick (Menu + Modo 3) ----------
const int pinJoyX = 32;
const int pinJoyY = 39;
const int pinJoySW = 4;

// ---------- Teclado matricial 4x4 (Modo 2) ----------
const byte FILAS = 4;
const byte COLUMNAS = 4;
char teclas[FILAS][COLUMNAS] = {
  { '1', '2', '3', '4' },
  { '5', '6', '7', '8' },
  { '9', 'A', 'B', 'C' },
  { 'D', 'E', 'F', 'G' }
};
byte pinesFilas[FILAS] = { 18, 19, 21, 22 };
byte pinesColumnas[COLUMNAS] = { 17, 16, 14, 13 };
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

char ordenTeclas[16] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G' };
int tonosTeclado[16] = { 220, 247, 262, 294, 330, 349, 392, 440, 494, 523, 587, 659, 698, 784, 880, 988 };

// ---------- Modo de juego (carrusel infinito) ----------
enum ModoJuego { MODO_BOTONES,
                 MODO_TECLADO,
                 MODO_JOYSTICK };
ModoJuego modoJuego = MODO_BOTONES;
int modoIndexActual = 0;  // se mantiene entre partidas, asi el carrusel no vuelve a 0 siempre
const char* nombresModo[3] = { "Botones", "Teclado 4x4", "Joystick" };

enum ZonaJoystick { ZONA_IZQUIERDA,
                    ZONA_CENTRO,
                    ZONA_DERECHA };

enum ZonaJoystickY { ZONA_ARRIBA,
                     ZONA_CENTRO_Y,
                     ZONA_ABAJO };

// ---------- Dificultad (eje Y, se mantiene como antes) ----------
enum Dificultad { FACIL,
                  MEDIO,
                  DIFICIL };
Dificultad dificultad = FACIL;
int dificultadIndexActual = 1;  // 0=Fácil, 1=Medio, 2=Difícil
const char* nombresDificultad[3] = { "Facil", "Medio", "Dificil" };

const int tiempoMuestraArr[3] = { 600, 400, 220 };
const int pausaArr[3] = { 300, 180, 100 };

// ---------- Secuencias ----------
int secuencia[100];
char secuenciaMatriz[100];
int nivelActual = 0;

// ---------- Variables para la pagina web ----------
String dificultadTexto = "-";
String estadoTexto = "Seleccionando modo y dificultad...";
String faseActual = "modo";  // "modo" | "dificultad" | "juego"

// Estado "en vivo" para dibujar botones / joystick / matriz en la web
int elementoActivo = -1;   // color 0=Verde,1=Amarillo,2=Rojo activo ahora mismo (-1 = ninguno) -> modos Botones y Joystick
char teclaActiva = 0;      // tecla activa ahora mismo en modo Teclado (0 = ninguna)
bool origenSimon = false;  // true = lo esta mostrando Simon | false = lo esta presionando el jugador
int joyXNorm = 0;          // posicion del joystick normalizada para la web: -100 (izquierda) .. 100 (derecha)
int joyYNorm = 0;          // posicion del joystick normalizada para la web: -100 (abajo) .. 100 (arriba)

// ---------- Servidor web ----------
const char* ssidAP = "SimonDice_ESP32";
const char* passAP = "simon1234";
WebServer server(80);

// ---------- Prototipos de funciones ----------
void iniciarAP();
void configurarServidor();
void handleRoot();
void handleEstado();
void esperarConServidor(unsigned long ms);
ZonaJoystick leerZonaX();
ZonaJoystickY leerZonaY();
void actualizarJoystickWeb();
void seleccionarModo();
void seleccionarDificultad();
void apagarTodo();
void iniciarNuevaSecuencia();
void jugarModoBotones();
int esperarBoton();
int indiceTecla(char c);
void jugarModoTeclado();
char esperarTecla();
void agregarSimboloMatriz();
int leerDireccionJoystick();
int esperarDireccionJoystick();
void jugarModoJoystick();
void agregarColorASecuencia();
void procesarResultado(bool acierto);
void animacionDeFallo();

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_COLORES; i++) {
    pinMode(pinesLED[i], OUTPUT);
    pinMode(pinesBoton[i], INPUT_PULLUP);
    digitalWrite(pinesLED[i], LOW);
  }
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinJoySW, INPUT_PULLUP);

  randomSeed(analogRead(39));

  iniciarAP();
  configurarServidor();

  Serial.println("=== SIMON DICE ===");
  seleccionarModo();
  seleccionarDificultad();
  iniciarNuevaSecuencia();
}

void loop() {
  server.handleClient();

  switch (modoJuego) {
    case MODO_BOTONES: jugarModoBotones(); break;
    case MODO_TECLADO: jugarModoTeclado(); break;
    case MODO_JOYSTICK: jugarModoJoystick(); break;
  }
}

// ================== ESPERA NO BLOQUEANTE ==================
void esperarConServidor(unsigned long ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < ms) {
    server.handleClient();
    delay(5);
  }
}

// ================== WEB SERVER ==================
void iniciarAP() {
  WiFi.softAP(ssidAP, passAP);
  Serial.print("Access Point creado. IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Conectate a 'SimonDice_ESP32' y entra a http://192.168.4.1");
}

void configurarServidor() {
  server.on("/", handleRoot);
  server.on("/estado", handleEstado);
  server.begin();
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Simon Dice - ESP32</title>
<style>
  :root{
    --bg:        #0a1510;
    --hueco:     #16261c;
    --panel:     #101f17;
    --borde:     #223a2c;
    --tinta:     #dcf5e4;
    --tinta-dim: #6f9481;
    --verde:     #3ddc71;
    --amarillo:  #ffcc33;
    --rojo:      #ff5252;
    --tele:      #57d1ff;
    --rail-plus: #e6453b;
    --rail-minus:#3b82c4;
    --font: ui-monospace, "SFMono-Regular", "Cascadia Mono", Consolas, "Roboto Mono", monospace;
  }

  * { box-sizing: border-box; }

  body {
    font-family: var(--font);
    color: var(--tinta);
    text-align: center;
    margin: 0;
    padding: 28px 16px 60px;
    background-color: var(--bg);
    background-image: radial-gradient(circle, var(--hueco) 1.6px, transparent 1.7px);
    background-size: 22px 22px;
    background-position: -4px -4px;
  }

  header { margin-bottom: 26px; }

  .power-dot {
    display:inline-block; width:9px; height:9px; border-radius:50%;
    background: var(--verde); box-shadow: 0 0 8px 2px var(--verde);
    margin-right: 10px; vertical-align: middle;
    animation: parpadeo 2.4s ease-in-out infinite;
  }
  @keyframes parpadeo { 0%,100% { opacity:1; } 50% { opacity:0.35; } }

  h1 {
    display:inline-flex; align-items:center;
    font-size: clamp(24px, 7vw, 32px);
    font-weight: 800;
    letter-spacing: 5px;
    margin: 0;
  }
  .subtitulo {
    color: var(--tinta-dim);
    font-size: 11px;
    letter-spacing: 2px;
    text-transform: uppercase;
    margin-top: 8px;
  }

  .rail { width: 130px; margin: 18px auto 0; display:flex; flex-direction:column; gap:4px; }
  .rail-linea { height: 3px; border-radius: 2px; background-repeat: repeat-x; background-size: 11px 3px; }
  .rail-mas  { background-image: radial-gradient(circle, var(--rail-plus) 1.4px, transparent 1.6px); }
  .rail-menos{ background-image: radial-gradient(circle, var(--rail-minus) 1.4px, transparent 1.6px); }

  /* --- Modulo (tarjeta tipo componente) --- */
  .modulo {
    position: relative;
    background: var(--panel);
    border: 1px solid var(--borde);
    border-radius: 10px;
    padding: 22px 22px 22px 28px;
    margin: 16px auto;
    width: 92%;
    max-width: 380px;
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.03), 0 10px 24px rgba(0,0,0,0.35);
  }
  .modulo::before {
    content: "";
    position: absolute; left: 8px; top: 12px; bottom: 12px; width: 4px; border-radius: 4px;
    background: linear-gradient(180deg, var(--verde), var(--amarillo), var(--rojo));
    opacity: 0.85;
  }
  .etiqueta {
    font-size: 11px; letter-spacing: 2px; text-transform: uppercase;
    color: var(--tinta-dim); margin-bottom: 16px; text-align:left;
  }
  .etiqueta::before { content: "// "; color: var(--tele); opacity:0.7; }
  .oculto { display: none !important; }

  /* --- LEDs de modo --- */
  .leds-modo { display: flex; justify-content: center; gap: 26px; }
  .led-item { display: flex; flex-direction: column; align-items: center; gap: 9px; opacity: 0.4; transition: opacity 0.3s; }
  .led-item.activo { opacity: 1; }
  .led-circulo {
    width: 44px; height: 44px; border-radius: 50%;
    background: radial-gradient(circle at 32% 28%, #3a3a3a, #1c1c1c 70%);
    transition: background 0.3s, box-shadow 0.3s;
  }
  .led-item.activo .led-circulo.verde    { background: radial-gradient(circle at 32% 28%, #a4ffc4, var(--verde) 55%); box-shadow: 0 0 18px 4px #3ddc71aa; animation: latido 1.1s ease-in-out infinite; }
  .led-item.activo .led-circulo.amarillo { background: radial-gradient(circle at 32% 28%, #fff2b8, var(--amarillo) 55%); box-shadow: 0 0 18px 4px #ffcc33aa; animation: latido 1.1s ease-in-out infinite; }
  .led-item.activo .led-circulo.rojo     { background: radial-gradient(circle at 32% 28%, #ffb3b3, var(--rojo) 55%); box-shadow: 0 0 18px 4px #ff5252aa; animation: latido 1.1s ease-in-out infinite; }
  @keyframes latido { 0%,100% { transform: scale(1); } 50% { transform: scale(1.12); } }
  .led-label { font-size: 11px; letter-spacing: 1px; text-transform: uppercase; color: var(--tinta-dim); }

  /* --- Badge "quien juega" --- */
  .quien-badge {
    display: inline-block; font-size: 10px; letter-spacing: 1px; text-transform: uppercase;
    padding: 4px 11px; border-radius: 20px; margin-bottom: 16px;
    background: #1c2620; color: var(--tinta-dim); transition: background 0.2s, color 0.2s;
  }
  .quien-badge.simon   { background: rgba(59,130,196,0.18); color: var(--tele); }
  .quien-badge.jugador { background: rgba(61,220,113,0.18); color: var(--verde); }

  /* --- Panel en vivo: Botones --- */
  .botones-fila { display: flex; justify-content: center; gap: 20px; }
  .boton-grande {
    width: 70px; height: 70px; border-radius: 50%;
    background: #1c2620; border: 3px solid var(--borde);
    transition: background 0.15s, box-shadow 0.15s, transform 0.15s, border-color 0.15s;
  }
  .boton-grande.on-rojo     { background: var(--rojo);    border-color: var(--rojo);    box-shadow: 0 0 26px 8px #ff5252cc; transform: scale(1.08); }
  .boton-grande.on-amarillo { background: var(--amarillo);border-color: var(--amarillo);box-shadow: 0 0 26px 8px #ffcc33cc; transform: scale(1.08); }
  .boton-grande.on-verde    { background: var(--verde);   border-color: var(--verde);   box-shadow: 0 0 26px 8px #3ddc71cc; transform: scale(1.08); }

  /* --- Panel en vivo: Joystick (imita el modulo PS2 real, con 4 tornillos de montaje) --- */
  .joy-plate {
    width: 190px; height: 190px; margin: 0 auto; border-radius: 14px;
    background: var(--panel); border: 1px solid var(--borde); position: relative;
    background-image:
      radial-gradient(circle at 12px 12px, var(--hueco) 3px, transparent 3.4px),
      radial-gradient(circle at calc(100% - 12px) 12px, var(--hueco) 3px, transparent 3.4px),
      radial-gradient(circle at 12px calc(100% - 12px), var(--hueco) 3px, transparent 3.4px),
      radial-gradient(circle at calc(100% - 12px) calc(100% - 12px), var(--hueco) 3px, transparent 3.4px);
  }
  .joy-marca { position: absolute; font-size: 10px; letter-spacing: 1px; text-transform: uppercase; color: var(--tinta-dim); }
  .joy-marca.arriba    { top: 10px; left: 50%; transform: translateX(-50%); }
  .joy-marca.izquierda { left: 14px; top: 50%; transform: translateY(-50%); }
  .joy-marca.derecha   { right: 14px; top: 50%; transform: translateY(-50%); }
  .joy-stick {
    position: absolute; top: 50%; left: 50%;
    width: 52px; height: 52px; border-radius: 50%;
    background: #26312a; border: 3px solid var(--borde);
    transform: translate(-50%, -50%);
    transition: transform 0.08s linear, background 0.15s, border-color 0.15s, box-shadow 0.15s;
  }
  .joy-stick.on-verde    { background: var(--verde);    border-color: var(--verde);    box-shadow: 0 0 22px 6px #3ddc71cc; }
  .joy-stick.on-amarillo { background: var(--amarillo); border-color: var(--amarillo); box-shadow: 0 0 22px 6px #ffcc33cc; }
  .joy-stick.on-rojo     { background: var(--rojo);     border-color: var(--rojo);     box-shadow: 0 0 22px 6px #ff5252cc; }

  /* --- Panel en vivo: Matriz 4x4 (imita teclas fisicas) --- */
  .matriz-grid {
    display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px;
    max-width: 250px; margin: 0 auto;
  }
  .tecla-cel {
    aspect-ratio: 1 / 1; border-radius: 8px;
    background: #1c2620; border: 1px solid var(--borde);
    box-shadow: 0 2px 0 rgba(0,0,0,0.4);
    display: flex; align-items: center; justify-content: center;
    font-size: 15px; font-weight: bold; color: var(--tinta-dim);
    transition: background 0.15s, border-color 0.15s, box-shadow 0.15s, color 0.15s, transform 0.1s;
  }
  .tecla-cel.on-simon   { background: rgba(59,130,196,0.25); border-color: var(--tele); color: var(--tele); box-shadow: 0 0 14px 2px rgba(87,209,255,0.5); transform: translateY(1px) scale(1.05); }
  .tecla-cel.on-jugador { background: rgba(61,220,113,0.25); border-color: var(--verde); color: var(--verde); box-shadow: 0 0 14px 2px rgba(61,220,113,0.5); transform: translateY(1px) scale(1.05); }

  /* --- Nivel: anillo tipo instrumento --- */
  .anillo-contenedor { position: relative; width: 132px; height: 132px; margin: 4px auto 0; }
  .anillo-contenedor svg { transform: rotate(-90deg); width: 132px; height: 132px; }
  .anillo-fondo { fill: none; stroke: var(--hueco); stroke-width: 9; }
  .anillo-progreso { fill: none; stroke: var(--tele); stroke-width: 9; stroke-linecap: round; transition: stroke-dashoffset 0.5s ease; filter: drop-shadow(0 0 5px rgba(87,209,255,0.6)); }
  .anillo-numero {
    position: absolute; top: 50%; left: 50%; transform: translate(-50%,-50%);
    font-size: 38px; font-weight: 800; color: var(--tele); font-variant-numeric: tabular-nums;
  }

  /* --- Dificultad: barras estilo VU-metro --- */
  .dificultad-barras { display: flex; justify-content: center; align-items: flex-end; gap: 16px; height: 66px; margin-top: 4px; }
  .barra-item { display: flex; flex-direction: column; align-items: center; gap: 9px; }
  .barra { width: 24px; border-radius: 5px 5px 0 0; background: var(--hueco); transition: background 0.3s, box-shadow 0.3s; }
  .barra.baja { height: 20px; }
  .barra.media { height: 40px; }
  .barra.alta { height: 60px; }
  #barraFacil.activa .barra   { background: linear-gradient(180deg, #a4ffc4, var(--verde));    box-shadow: 0 0 14px #3ddc7188; }
  #barraMedio.activa .barra   { background: linear-gradient(180deg, #fff2b8, var(--amarillo));  box-shadow: 0 0 14px #ffcc3388; }
  #barraDificil.activa .barra { background: linear-gradient(180deg, #ffb3b3, var(--rojo));      box-shadow: 0 0 14px #ff525288; }
  .barra-nombre { font-size: 10px; letter-spacing: 1px; text-transform: uppercase; color: var(--tinta-dim); }

  /* --- Estado --- */
  .estado-banner {
    font-size: 14px; font-weight: bold; letter-spacing: 1px; text-transform: uppercase;
    padding: 14px; border-radius: 8px;
    background: var(--hueco); color: var(--tinta-dim); transition: background 0.3s, color 0.3s;
  }
  .estado-banner.jugando   { background: rgba(59,130,196,0.18); color: var(--tele); }
  .estado-banner.correcto  { background: rgba(61,220,113,0.18); color: var(--verde); }
  .estado-banner.fallaste  { background: rgba(255,82,82,0.18);  color: var(--rojo); }
  .estado-banner.espera    { background: var(--hueco); color: var(--tinta-dim); }

  footer {
    margin-top: 30px; font-size: 10px; letter-spacing: 1px;
    color: var(--tinta-dim); opacity: 0.6; text-transform: uppercase;
  }

  @media (prefers-reduced-motion: reduce) {
    *, *::before, *::after {
      animation-duration: 0.001ms !important;
      animation-iteration-count: 1 !important;
      transition-duration: 0.001ms !important;
    }
  }
</style>
</head>
<body>
  <header>
    <h1><span class="power-dot"></span>SIMON DICE</h1>
    <div class="subtitulo">ESP32 // Modo Access Point</div>
    <div class="rail">
      <div class="rail-linea rail-mas"></div>
      <div class="rail-linea rail-menos"></div>
    </div>
  </header>

  <div class="modulo" id="moduloModo">
    <div class="etiqueta">Modo de juego</div>
    <div class="leds-modo">
      <div class="led-item" id="ledTeclado">
        <div class="led-circulo rojo"></div>
        <div class="led-label">Teclado 4x4</div>
      </div>
      <div class="led-item" id="ledJoystick">
        <div class="led-circulo amarillo"></div>
        <div class="led-label">Joystick</div>
      </div>
            <div class="led-item" id="ledBotones">
        <div class="led-circulo verde"></div>
        <div class="led-label">Botones</div>
      </div>
    </div>
  </div>

  <!-- Panel en vivo: BOTONES -->
  <div class="modulo oculto" id="panelBotones">
    <div class="etiqueta">Botones en vivo</div>
    <div class="quien-badge" id="quienBotones">-</div>
    <div class="botones-fila">
      <div class="boton-grande" id="btnVerde"></div>
      <div class="boton-grande" id="btnAmarillo"></div>
      <div class="boton-grande" id="btnRojo"></div>
    </div>
  </div>

  <!-- Panel en vivo: JOYSTICK -->
  <div class="modulo oculto" id="panelJoystick">
    <div class="etiqueta">Joystick en vivo</div>
    <div class="quien-badge" id="quienJoystick">-</div>
    <div class="joy-plate">
      <div class="joy-marca arriba">Amarillo</div>
      <div class="joy-marca izquierda">Rojo</div>
      <div class="joy-marca derecha">Verde</div>
      <div class="joy-stick" id="joyStick"></div>
    </div>
  </div>

  <!-- Panel en vivo: MATRIZ 4x4 -->
  <div class="modulo oculto" id="panelMatriz">
    <div class="etiqueta">Teclado 4x4 en vivo</div>
    <div class="quien-badge" id="quienMatriz">-</div>
    <div class="matriz-grid" id="matrizGrid"></div>
  </div>

  <div class="modulo oculto" id="moduloNivel">
    <div class="etiqueta">Nivel actual</div>
    <div class="anillo-contenedor">
      <svg viewBox="0 0 140 140">
        <circle class="anillo-fondo" cx="70" cy="70" r="60"></circle>
        <circle class="anillo-progreso" id="anilloProgreso" cx="70" cy="70" r="60"
                stroke-dasharray="377" stroke-dashoffset="377"></circle>
      </svg>
      <div class="anillo-numero" id="nivelNumero">0</div>
    </div>
  </div>

  <div class="modulo" id="moduloDificultad">
    <div class="etiqueta">Dificultad</div>
    <div class="dificultad-barras">
      <div class="barra-item" id="barraFacil">
        <div class="barra baja"></div>
        <div class="barra-nombre">Facil</div>
      </div>
      <div class="barra-item" id="barraMedio">
        <div class="barra media"></div>
        <div class="barra-nombre">Medio</div>
      </div>
      <div class="barra-item" id="barraDificil">
        <div class="barra alta"></div>
        <div class="barra-nombre">Dificil</div>
      </div>
    </div>
  </div>

  <div class="modulo">
    <div class="etiqueta">Estado</div>
    <div class="estado-banner espera" id="estadoBanner">-</div>
  </div>

  <footer>192.168.4.1 // SimonDice_ESP32</footer>

<script>
// Layout del teclado matricial, igual al del ESP32
const teclasMatriz = [
  ['1','2','3','4'],
  ['5','6','7','8'],
  ['9','A','B','C'],
  ['D','E','F','G']
];

function numeroDeTecla(t) {
  if (t >= '1' && t <= '9') return t;
  return String(10 + (t.charCodeAt(0) - 'A'.charCodeAt(0)));
}

// Construir la grilla 4x4 una sola vez
const grid = document.getElementById('matrizGrid');
teclasMatriz.flat().forEach(function (t) {
  const cel = document.createElement('div');
  cel.className = 'tecla-cel';
  cel.dataset.tecla = t;
  cel.innerText = numeroDeTecla(t);
  grid.appendChild(cel);
});

const nombreColor = ['verde', 'amarillo', 'rojo']; // 0,1,2 igual que en el ESP32

function actualizar() {
  fetch('/estado')
    .then(function (r) { return r.json(); })
    .then(function (datos) {

      // --- Mostrar/ocultar módulos según fase ---
      document.getElementById('moduloModo').classList.toggle('oculto', datos.fase !== 'modo');
      document.getElementById('moduloDificultad').classList.toggle('oculto', datos.fase !== 'dificultad');
      document.getElementById('moduloNivel').classList.toggle('oculto', datos.fase !== 'juego');

      const enJuego = datos.fase === 'juego';

      // --- LEDs de modo ---
      const mapeoVisual = { 0: "ledBotones", 1: "ledTeclado", 2: "ledJoystick" };
      ["ledBotones", "ledJoystick", "ledTeclado"].forEach(function (id) {
        document.getElementById(id).classList.remove("activo");
      });
      const idActivo = mapeoVisual[datos.modoIndex];
      if (idActivo) document.getElementById(idActivo).classList.add("activo");

      // --- Mostrar solo el panel en vivo del modo actual ---
      document.getElementById('panelBotones').classList.toggle('oculto', !(enJuego && datos.modoIndex === 0));
      document.getElementById('panelMatriz').classList.toggle('oculto', !(enJuego && datos.modoIndex === 1));
      document.getElementById('panelJoystick').classList.toggle('oculto', !(enJuego && datos.modoIndex === 2));

      const quien = datos.origenSimon ? 'Simon' : 'Tu turno';
      const claseQuien = datos.origenSimon ? 'simon' : 'jugador';

      // --- Panel Botones ---
      if (datos.modoIndex === 0) {
        const q = document.getElementById('quienBotones');
        q.innerText = quien; q.className = 'quien-badge ' + claseQuien;

        ['btnVerde', 'btnAmarillo', 'btnRojo'].forEach(function (id, i) {
          const el = document.getElementById(id);
          el.className = 'boton-grande';
          if (datos.elementoActivo === i) el.classList.add('on-' + nombreColor[i]);
        });
      }

      // --- Panel Joystick ---
      if (datos.modoIndex === 2) {
        const q = document.getElementById('quienJoystick');
        q.innerText = quien; q.className = 'quien-badge ' + claseQuien;

        const stick = document.getElementById('joyStick');
        // joyX: -100 izquierda .. 100 derecha | joyY: -100 abajo .. 100 arriba
        const radio = 55; // recorrido maximo del stick en px
        const dx = (datos.joyX / 100) * radio;
        const dy = (datos.joyY / 100) * radio;
        stick.style.transform = 'translate(calc(-50% + ' + dx + 'px), calc(-50% - ' + dy + 'px))';

        stick.classList.remove('on-verde', 'on-amarillo', 'on-rojo');
        if (datos.elementoActivo >= 0) stick.classList.add('on-' + nombreColor[datos.elementoActivo]);
      }

      // --- Panel Matriz 4x4 ---
      if (datos.modoIndex === 1) {
        const q = document.getElementById('quienMatriz');
        q.innerText = quien; q.className = 'quien-badge ' + claseQuien;

        document.querySelectorAll('.tecla-cel').forEach(function (cel) {
          cel.classList.remove('on-simon', 'on-jugador');
          if (datos.teclaActiva && cel.dataset.tecla === datos.teclaActiva) {
            cel.classList.add(datos.origenSimon ? 'on-simon' : 'on-jugador');
          }
        });
      }

      // --- Nivel (anillo de progreso, ciclo cada 10 niveles) ---
      const nivelVisual = datos.nivel % 10;
      const circunferencia = 377;
      const offset = circunferencia - (nivelVisual / 10) * circunferencia;
      document.getElementById('anilloProgreso').style.strokeDashoffset = offset;
      document.getElementById('nivelNumero').innerText = datos.nivel;

      // --- Dificultad (barras) ---
      const barras = ["barraFacil", "barraMedio", "barraDificil"];
      barras.forEach(function (id) { document.getElementById(id).classList.remove("activa"); });
      if (datos.dificultadIndex !== undefined && barras[datos.dificultadIndex]) {
        document.getElementById(barras[datos.dificultadIndex]).classList.add("activa");
      }

      // --- Estado (banner con color) ---
      const banner = document.getElementById('estadoBanner');
      banner.innerText = datos.estado;
      banner.className = "estado-banner";
      if (datos.estado.indexOf("Correcto") !== -1) banner.classList.add("correcto");
      else if (datos.estado.indexOf("Fallaste") !== -1) banner.classList.add("fallaste");
      else if (datos.estado.indexOf("turno") !== -1 || datos.estado.indexOf("Mostrando") !== -1 || datos.estado.indexOf("Jugando") !== -1) banner.classList.add("jugando");
      else banner.classList.add("espera");
    });
}

setInterval(actualizar, 120);
actualizar();
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleEstado() {
  String teclaActivaStr = (teclaActiva == 0) ? "" : String(teclaActiva);

  String json = "{";
  json += "\"modo\":\"" + String(nombresModo[modoIndexActual]) + "\",";
  json += "\"modoIndex\":" + String(modoIndexActual) + ",";
  json += "\"dificultad\":\"" + dificultadTexto + "\",";
  json += "\"dificultadIndex\":" + String((int)dificultad) + ",";
  json += "\"estado\":\"" + estadoTexto + "\",";
  json += "\"nivel\":" + String(nivelActual) + ",";
  json += "\"elementoActivo\":" + String(elementoActivo) + ",";
  json += "\"teclaActiva\":\"" + teclaActivaStr + "\",";
  json += "\"origenSimon\":" + String(origenSimon ? "true" : "false") + ",";
  json += "\"fase\":\"" + faseActual + "\",";
  json += "\"joyX\":" + String(joyXNorm) + ",";
  json += "\"joyY\":" + String(joyYNorm);
  json += "}";
  server.send(200, "application/json", json);
}

// ================== ZONA DEL JOYSTICK (eje X) ==================
ZonaJoystick leerZonaX() {
  int x = analogRead(pinJoyX);
  if (x > 2900) return ZONA_IZQUIERDA;
  if (x < 1200) return ZONA_DERECHA;
  return ZONA_CENTRO;
}

ZonaJoystickY leerZonaY() {
  int y = analogRead(pinJoyY);
  if (y < 1200) return ZONA_ARRIBA;  // fisico arriba
  if (y > 2900) return ZONA_ABAJO;   // fisico abajo
  return ZONA_CENTRO_Y;
}

// Lee el joystick fisico y actualiza joyXNorm/joyYNorm (-100..100)
// para que la pagina web pueda dibujar el "stick" en tiempo real.
void actualizarJoystickWeb() {
  int xRaw = analogRead(pinJoyX);
  int yRaw = analogRead(pinJoyY);
  joyXNorm = map(xRaw, 0, 4095, -100, 100);  // xRaw alto (fisico) = derecha -> positivo en pantalla
  joyYNorm = map(yRaw, 0, 4095, 100, -100);  // yRaw alto (fisico) = abajo   -> negativo en pantalla
}

// ================== MENU: SELECCIÓN DE MODO ==================
void seleccionarModo() {
  faseActual = "modo";
  estadoTexto = "Seleccionando modo...";
  elementoActivo = -1;
  teclaActiva = 0;
  ZonaJoystick zonaAnterior = leerZonaX();

  while (true) {
    server.handleClient();
    actualizarJoystickWeb();

    ZonaJoystick zonaActual = leerZonaX();
    if (zonaAnterior == ZONA_CENTRO && zonaActual == ZONA_DERECHA) {
      modoIndexActual = (modoIndexActual + 1) % 3;
    } else if (zonaAnterior == ZONA_CENTRO && zonaActual == ZONA_IZQUIERDA) {
      modoIndexActual = (modoIndexActual + 2) % 3;
    }
    zonaAnterior = zonaActual;

    const int ledParaModo[3] = { 0, 2, 1 };
    apagarTodo();
    digitalWrite(pinesLED[ledParaModo[modoIndexActual]], HIGH);

    if (digitalRead(pinJoySW) == LOW) {
      Serial.print("Modo: ");
      Serial.println(nombresModo[modoIndexActual]);
      esperarConServidor(400);
      apagarTodo();
      return;
    }
    delay(40);
  }
}

// ================== MENU: SELECCIÓN DE DIFICULTAD ==================
void seleccionarDificultad() {
  faseActual = "dificultad";
  estadoTexto = "Seleccionando dificultad...";
  elementoActivo = -1;
  teclaActiva = 0;

  // Mapeo: Fácil -> Verde (0), Medio -> Amarillo (1), Difícil -> Rojo (2)
  const int ledParaDificultad[3] = { 0, 1, 2 };
  ZonaJoystickY zonaYAnterior = leerZonaY();

  while (true) {
    server.handleClient();
    actualizarJoystickWeb();

    ZonaJoystickY zonaYActual = leerZonaY();
    if (zonaYAnterior == ZONA_CENTRO_Y && zonaYActual == ZONA_ARRIBA) {
      dificultadIndexActual = (dificultadIndexActual + 1) % 3;  // arriba -> hacia Dificil
    } else if (zonaYAnterior == ZONA_CENTRO_Y && zonaYActual == ZONA_ABAJO) {
      dificultadIndexActual = (dificultadIndexActual + 2) % 3;  // abajo -> hacia Facil
    }
    zonaYAnterior = zonaYActual;

    dificultad = (Dificultad)dificultadIndexActual;
    dificultadTexto = nombresDificultad[dificultadIndexActual];

    // Encender LED correspondiente a la dificultad
    apagarTodo();
    digitalWrite(pinesLED[ledParaDificultad[dificultadIndexActual]], HIGH);

    if (digitalRead(pinJoySW) == LOW) {
      modoJuego = (ModoJuego)modoIndexActual;
      faseActual = "juego";
      estadoTexto = "Jugando";
      Serial.print("Dificultad: ");
      Serial.println(nombresDificultad[dificultadIndexActual]);
      esperarConServidor(400);
      apagarTodo();
      return;
    }
    delay(40);
  }
}

void apagarTodo() {
  for (int i = 0; i < NUM_COLORES; i++) digitalWrite(pinesLED[i], LOW);
}

void iniciarNuevaSecuencia() {
  nivelActual = 0;
  if (modoJuego == MODO_TECLADO) agregarSimboloMatriz();
  else agregarColorASecuencia();
}

// ================== MODO 1: BOTONES ==================
void jugarModoBotones() {
  int tiempoMuestra = tiempoMuestraArr[dificultad];
  int pausa = pausaArr[dificultad];

  estadoTexto = "Mostrando secuencia...";
  esperarConServidor(500);
  for (int i = 0; i < nivelActual; i++) {
    int color = secuencia[i];
    elementoActivo = color;
    origenSimon = true;
    digitalWrite(pinesLED[color], HIGH);
    tone(pinBuzzer, tonos[color]);
    esperarConServidor(tiempoMuestra);
    digitalWrite(pinesLED[color], LOW);
    noTone(pinBuzzer);
    elementoActivo = -1;
    esperarConServidor(pausa);
  }

  estadoTexto = "Tu turno...";
  bool acierto = true;
  for (int i = 0; i < nivelActual; i++) {
    int boton = esperarBoton();
    elementoActivo = boton;
    origenSimon = false;
    digitalWrite(pinesLED[boton], HIGH);
    tone(pinBuzzer, tonos[boton]);
    esperarConServidor(200);
    digitalWrite(pinesLED[boton], LOW);
    noTone(pinBuzzer);
    elementoActivo = -1;
    if (boton != secuencia[i]) {
      acierto = false;
      break;
    }
  }

  procesarResultado(acierto);
}

int esperarBoton() {
  while (true) {
    server.handleClient();
    for (int i = 0; i < NUM_COLORES; i++) {
      if (digitalRead(pinesBoton[i]) == LOW) {
        elementoActivo = i;
        origenSimon = false;
        delay(30);
        while (digitalRead(pinesBoton[i]) == LOW) { server.handleClient(); }
        return i;
      }
    }
    delay(10);
  }
}

// ================== MODO 2: TECLADO MATRICIAL ==================
int indiceTecla(char c) {
  for (int i = 0; i < 16; i++)
    if (ordenTeclas[i] == c) return i;
  return -1;
}

void jugarModoTeclado() {
  int tiempoMuestra = tiempoMuestraArr[dificultad];
  int pausa = pausaArr[dificultad];

  estadoTexto = "Mostrando secuencia...";
  esperarConServidor(500);
  for (int i = 0; i < nivelActual; i++) {
    char tecla = secuenciaMatriz[i];
    int idx = indiceTecla(tecla);
    teclaActiva = tecla;
    origenSimon = true;
    apagarTodo();
    for (int j = 0; j < NUM_COLORES; j++) digitalWrite(pinesLED[j], HIGH);
    tone(pinBuzzer, tonosTeclado[idx]);
    esperarConServidor(tiempoMuestra);
    apagarTodo();
    noTone(pinBuzzer);
    teclaActiva = 0;
    esperarConServidor(pausa);
  }

  estadoTexto = "Tu turno...";
  bool acierto = true;
  for (int i = 0; i < nivelActual; i++) {
    char tecla = esperarTecla();
    int idx = indiceTecla(tecla);
    teclaActiva = tecla;
    origenSimon = false;
    tone(pinBuzzer, tonosTeclado[idx], 200);
    esperarConServidor(200);
    teclaActiva = 0;
    if (tecla != secuenciaMatriz[i]) {
      acierto = false;
      break;
    }
  }

  procesarResultado(acierto);
}

char esperarTecla() {
  char tecla = teclado.getKey();
  while (tecla == NO_KEY) {
    server.handleClient();
    tecla = teclado.getKey();
    delay(10);
  }
  return tecla;
}

void agregarSimboloMatriz() {
  int idx = random(0, 16);
  secuenciaMatriz[nivelActual] = ordenTeclas[idx];
  nivelActual++;
}

// ================== MODO 3: JOYSTICK ==================
// Derecha = Verde (0) | Arriba = Amarillo (1) | Izquierda = Rojo (2)
int leerDireccionJoystick() {
  int x = analogRead(pinJoyX);
  int y = analogRead(pinJoyY);

  const int centro = 2048;
  const int umbral = 1000;     // que tan lejos del centro para considerar "presionado"
  const int zonaMuerta = 600;  // el eje contrario debe estar dentro de esto para que cuente

  bool xDerecha = x > (centro + umbral);
  bool xIzquierda = x < (centro - umbral);
  bool yArriba = y < (centro - umbral);

  bool xEnZonaMuerta = abs(x - centro) < zonaMuerta;
  bool yEnZonaMuerta = abs(y - centro) < zonaMuerta;

  if (yArriba && xEnZonaMuerta) return 1;     // arriba -> Amarillo, solo si X esta quieto
  if (xIzquierda && yEnZonaMuerta) return 2;  // izquierda -> Rojo, solo si Y esta quieto
  if (xDerecha && yEnZonaMuerta) return 0;    // derecha -> Verde, solo si Y esta quieto

  return -1;
}

int esperarDireccionJoystick() {
  while (leerDireccionJoystick() != -1) {
    server.handleClient();
    actualizarJoystickWeb();
    delay(10);
  }
  int direccion = -1;
  while (direccion == -1) {
    server.handleClient();
    actualizarJoystickWeb();
    direccion = leerDireccionJoystick();
    delay(10);
  }
  delay(50);
  return direccion;
}

void jugarModoJoystick() {
  int tiempoMuestra = tiempoMuestraArr[dificultad];
  int pausa = pausaArr[dificultad];

  estadoTexto = "Mostrando secuencia...";
  esperarConServidor(500);
  for (int i = 0; i < nivelActual; i++) {
    int color = secuencia[i];
    elementoActivo = color;
    origenSimon = true;

    if (color == 1) {
      joyXNorm = 0;
      joyYNorm = 100;
    }  // Amarillo -> Arriba
    else if (color == 2) {
      joyXNorm = -100;
      joyYNorm = 0;
    }  // Rojo -> Izquierda
    else {
      joyXNorm = 100;
      joyYNorm = 0;
    }  // Verde -> Derecha

    digitalWrite(pinesLED[color], HIGH);
    tone(pinBuzzer, tonos[color]);
    esperarConServidor(tiempoMuestra);
    digitalWrite(pinesLED[color], LOW);
    noTone(pinBuzzer);
    elementoActivo = -1;
    joyXNorm = 0;
    joyYNorm = 0;
    esperarConServidor(pausa);
  }

  estadoTexto = "Tu turno...";
  bool acierto = true;
  for (int i = 0; i < nivelActual; i++) {
    int direccion = esperarDireccionJoystick();
    elementoActivo = direccion;
    origenSimon = false;
    digitalWrite(pinesLED[direccion], HIGH);
    tone(pinBuzzer, tonos[direccion]);
    esperarConServidor(200);
    digitalWrite(pinesLED[direccion], LOW);
    noTone(pinBuzzer);
    elementoActivo = -1;
    joyXNorm = 0;
    joyYNorm = 0;
    if (direccion != secuencia[i]) {
      acierto = false;
      break;
    }
  }

  procesarResultado(acierto);
}

// ================== COMUN ==================
void agregarColorASecuencia() {
  secuencia[nivelActual] = random(0, NUM_COLORES);
  nivelActual++;
}

void procesarResultado(bool acierto) {
  if (acierto) {
    estadoTexto = "Correcto!";
    Serial.print("Bien! Nivel: ");
    Serial.println(nivelActual + 1);
    esperarConServidor(600);
    if (modoJuego == MODO_TECLADO) agregarSimboloMatriz();
    else agregarColorASecuencia();
  } else {
    estadoTexto = "Fallaste!";
    animacionDeFallo();
    Serial.println("Fallaste! Volviendo al menu...");
    esperarConServidor(1200);
    seleccionarModo();
    seleccionarDificultad();
    iniciarNuevaSecuencia();
  }
}

void animacionDeFallo() {
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < NUM_COLORES; i++) digitalWrite(pinesLED[i], HIGH);
    tone(pinBuzzer, 100);
    esperarConServidor(200);
    for (int i = 0; i < NUM_COLORES; i++) digitalWrite(pinesLED[i], LOW);
    noTone(pinBuzzer);
    esperarConServidor(200);
  }
}
