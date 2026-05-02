import {
  clear, fill_rect, draw_rect, draw_line_v, draw_str, draw_str_big,
  rgb, hsv, dim_color, lerp_color,
  sinf, cosf, absf, rand, srand,
  width, height, get_frame, millis,
  get_gesture, get_touch_y, is_pressed,
  GESTURE_TAP,
  log
} from "../pixelitl";

// --- Constants ---
const PADDLE_W: i32 = 1;
const PADDLE_H: i32 = 10;
const BALL_SIZE: i32 = 2;
const PADDLE_MARGIN: i32 = 3;
const PADDLE_SPEED: f32 = 0.03;
const AI_SPEED: f32 = 0.022;
const BALL_SPEED_INIT: f32 = 0.025;
const BALL_SPEED_INC: f32 = 0.001;
const MAX_SCORE: i32 = 5;

// --- State ---
let screenW: i32 = 80;
let screenH: i32 = 48;

// Paddle Y positions (center)
let playerY: f32 = 24.0;
let aiY: f32 = 24.0;

// Ball position and velocity
let ballX: f32 = 40.0;
let ballY: f32 = 24.0;
let ballVx: f32 = 0.02;
let ballVy: f32 = 0.012;
let ballSpeed: f32 = BALL_SPEED_INIT;

// Score
let playerScore: i32 = 0;
let aiScore: i32 = 0;

// Game state
let gameOver: bool = false;
let serveTimer: i32 = 0;
let serving: bool = true;
let rallyCount: i32 = 0;

// Trail effect
const TRAIL_LEN: i32 = 6;
let trailX: StaticArray<f32> = new StaticArray<f32>(TRAIL_LEN);
let trailY: StaticArray<f32> = new StaticArray<f32>(TRAIL_LEN);
let trailIdx: i32 = 0;
let trailTick: i32 = 0;

// Colors
let colBg: u32 = 0;
let colField: u32 = 0;
let colPlayer: u32 = 0;
let colAi: u32 = 0;
let colBall: u32 = 0;
let colScore: u32 = 0;
let colNet: u32 = 0;

// Time
let t: f32 = 0;

function resetBall(towardsPlayer: bool): void {
  ballX = <f32>(screenW / 2);
  ballY = <f32>(screenH / 2);
  const angle: f32 = (<f32>(rand() % 60) - 30.0) * 0.0174533; // ±30 degrees
  const dir: f32 = towardsPlayer ? -1.0 : 1.0;
  ballSpeed = BALL_SPEED_INIT + <f32>((playerScore + aiScore)) * BALL_SPEED_INC;
  ballVx = cosf(angle) * ballSpeed * dir;
  ballVy = sinf(angle) * ballSpeed;
  serving = true;
  serveTimer = 800; // ms pause before serve
  rallyCount = 0;

  // Clear trail
  for (let i: i32 = 0; i < TRAIL_LEN; i++) {
    unchecked(trailX[i] = ballX);
    unchecked(trailY[i] = ballY);
  }
}

function resetGame(): void {
  playerScore = 0;
  aiScore = 0;
  gameOver = false;
  playerY = <f32>(screenH / 2);
  aiY = <f32>(screenH / 2);
  resetBall(false);
}

export function app_init(): void {
  screenW = width();
  screenH = height();
  srand(<i32>millis());

  colBg = rgb(5, 5, 15);
  colField = rgb(10, 10, 30);
  colPlayer = rgb(80, 200, 255);
  colAi = rgb(255, 100, 80);
  colBall = rgb(255, 255, 255);
  colScore = rgb(40, 40, 80);
  colNet = rgb(20, 20, 50);

  resetGame();
}

export function app_update(frame: i32, dt: i32): void {
  t += <f32>dt * 0.001;
  const dtf: f32 = <f32>dt;

  // Handle game over - tap to restart
  if (gameOver) {
    if (get_gesture() == GESTURE_TAP) {
      resetGame();
    }
    return;
  }

  // Serve countdown
  if (serving) {
    serveTimer -= dt;
    if (serveTimer <= 0) {
      serving = false;
    }
    return;
  }

  // --- Player input ---
  if (is_pressed()) {
    // Map touch Y (0..47) to paddle position
    const touchTarget: f32 = <f32>get_touch_y();
    if (touchTarget < playerY) {
      playerY -= PADDLE_SPEED * dtf;
    } else if (touchTarget > playerY) {
      playerY += PADDLE_SPEED * dtf;
    }
  }

  // Clamp player paddle
  const halfPaddle: f32 = <f32>(PADDLE_H / 2);
  if (playerY < halfPaddle) playerY = halfPaddle;
  if (playerY > <f32>screenH - halfPaddle) playerY = <f32>screenH - halfPaddle;

  // --- AI ---
  const aiTarget: f32 = ballY;
  const aiDiff: f32 = aiTarget - aiY;
  // AI gets faster with rally count
  const aiSpeedMod: f32 = AI_SPEED + <f32>rallyCount * 0.001;
  if (absf(aiDiff) > 1.0) {
    if (aiDiff > 0) {
      aiY += aiSpeedMod * dtf;
    } else {
      aiY -= aiSpeedMod * dtf;
    }
  }
  // Clamp AI paddle
  if (aiY < halfPaddle) aiY = halfPaddle;
  if (aiY > <f32>screenH - halfPaddle) aiY = <f32>screenH - halfPaddle;

  // --- Ball movement ---
  ballX += ballVx * dtf;
  ballY += ballVy * dtf;

  // Store trail
  trailTick++;
  if (trailTick >= 2) {
    trailTick = 0;
    unchecked(trailX[trailIdx] = ballX);
    unchecked(trailY[trailIdx] = ballY);
    trailIdx = (trailIdx + 1) % TRAIL_LEN;
  }

  // --- Wall bounce (top/bottom) ---
  if (ballY <= 1.0) {
    ballY = 1.0;
    ballVy = -ballVy;
  }
  if (ballY >= <f32>(screenH - BALL_SIZE - 1)) {
    ballY = <f32>(screenH - BALL_SIZE - 1);
    ballVy = -ballVy;
  }

  // --- Paddle collision (player - left side) ---
  const playerPaddleX: f32 = <f32>PADDLE_MARGIN;
  const playerPaddleTop: f32 = playerY - halfPaddle;
  const playerPaddleBot: f32 = playerY + halfPaddle;

  if (ballVx < 0 && ballX <= playerPaddleX + <f32>PADDLE_W + 1.0 && ballX >= playerPaddleX) {
    if (ballY + <f32>BALL_SIZE > playerPaddleTop && ballY < playerPaddleBot) {
      // Hit! Reflect with angle based on hit position
      const hitPos: f32 = (ballY - playerY) / halfPaddle; // -1 to 1
      ballVx = absf(ballVx) + 0.001;
      ballVy = hitPos * ballSpeed * 1.5;
      ballX = playerPaddleX + <f32>PADDLE_W + 1.0;
      rallyCount++;
    }
  }

  // --- Paddle collision (AI - right side) ---
  const aiPaddleX: f32 = <f32>(screenW - PADDLE_MARGIN - PADDLE_W);
  const aiPaddleTop: f32 = aiY - halfPaddle;
  const aiPaddleBot: f32 = aiY + halfPaddle;

  if (ballVx > 0 && ballX + <f32>BALL_SIZE >= aiPaddleX && ballX <= aiPaddleX + <f32>PADDLE_W) {
    if (ballY + <f32>BALL_SIZE > aiPaddleTop && ballY < aiPaddleBot) {
      const hitPos: f32 = (ballY - aiY) / halfPaddle;
      ballVx = -(absf(ballVx) + 0.001);
      ballVy = hitPos * ballSpeed * 1.5;
      ballX = aiPaddleX - <f32>BALL_SIZE;
      rallyCount++;
    }
  }

  // --- Scoring ---
  if (ballX < -5.0) {
    // AI scores
    aiScore++;
    if (aiScore >= MAX_SCORE) {
      gameOver = true;
    } else {
      resetBall(false);
    }
  }

  if (ballX > <f32>(screenW + 5)) {
    // Player scores
    playerScore++;
    if (playerScore >= MAX_SCORE) {
      gameOver = true;
    } else {
      resetBall(true);
    }
  }
}

export function app_draw(): void {
  // Background
  clear(colBg);

  const halfPaddle: i32 = PADDLE_H / 2;

  // Draw center net (dashed line)
  const cx: i32 = screenW / 2;
  for (let y: i32 = 0; y < screenH; y += 3) {
    fill_rect(cx, y, 1, 2, colNet);
  }

  // Draw scores
  const scoreStr = playerScore.toString() + "   " + aiScore.toString();
  draw_str(cx - 10, 2, scoreStr, colScore);

  // Draw player paddle (left)
  const ppY: i32 = <i32>playerY - halfPaddle;
  fill_rect(PADDLE_MARGIN, ppY, PADDLE_W + 1, PADDLE_H, colPlayer);
  // Paddle glow edge
  fill_rect(PADDLE_MARGIN + PADDLE_W + 1, ppY + 1, 1, PADDLE_H - 2, dim_color(colPlayer, 0.3));

  // Draw AI paddle (right)
  const apY: i32 = <i32>aiY - halfPaddle;
  const aiPX: i32 = screenW - PADDLE_MARGIN - PADDLE_W - 1;
  fill_rect(aiPX, apY, PADDLE_W + 1, PADDLE_H, colAi);
  // Paddle glow edge
  fill_rect(aiPX - 1, apY + 1, 1, PADDLE_H - 2, dim_color(colAi, 0.3));

  if (serving) {
    // Blink ball during serve
    const blink: i32 = (<i32>(t * 8.0)) & 1;
    if (blink) {
      fill_rect(<i32>ballX, <i32>ballY, BALL_SIZE, BALL_SIZE, dim_color(colBall, 0.4));
    }
    draw_str(cx - 12, screenH / 2 - 4, "READY", dim_color(colBall, 0.5));
    return;
  }

  // Draw ball trail
  for (let i: i32 = 0; i < TRAIL_LEN; i++) {
    const idx: i32 = (trailIdx + i) % TRAIL_LEN;
    const age: f32 = <f32>i / <f32>TRAIL_LEN;
    const trColor: u32 = dim_color(colBall, age * 0.15);
    fill_rect(<i32>unchecked(trailX[idx]), <i32>unchecked(trailY[idx]), BALL_SIZE, BALL_SIZE, trColor);
  }

  // Draw ball
  fill_rect(<i32>ballX, <i32>ballY, BALL_SIZE, BALL_SIZE, colBall);
  // Ball glow
  const glowCol: u32 = dim_color(colBall, 0.2);
  fill_rect(<i32>ballX - 1, <i32>ballY, 1, BALL_SIZE, glowCol);
  fill_rect(<i32>ballX + BALL_SIZE, <i32>ballY, 1, BALL_SIZE, glowCol);
  fill_rect(<i32>ballX, <i32>ballY - 1, BALL_SIZE, 1, glowCol);
  fill_rect(<i32>ballX, <i32>ballY + BALL_SIZE, BALL_SIZE, 1, glowCol);

  // Game over overlay
  if (gameOver) {
    // Dim background
    fill_rect(10, 14, 60, 20, rgb(0, 0, 0));
    draw_rect(10, 14, 60, 20, dim_color(colBall, 0.3));

    if (playerScore >= MAX_SCORE) {
      draw_str(18, 18, "YOU WIN!", hsv(t * 90.0, 1.0, 1.0));
    } else {
      draw_str(16, 18, "YOU LOSE", colAi);
    }
    draw_str(18, 27, "TAP PLAY", dim_color(colBall, 0.5));
  }
}
