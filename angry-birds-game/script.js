const canvas = document.getElementById("game");
const ctx = canvas.getContext("2d");
const birdsLeftEl = document.getElementById("birds-left");
const pigsLeftEl = document.getElementById("pigs-left");
const hintEl = document.getElementById("hint");
const resetButton = document.getElementById("reset");

const slingAnchor = { x: 160, y: 360 };
const gravity = 0.35;
const maxPull = 90;
const baseRadius = 18;

const state = {
  birdsLeft: 3,
  pigsLeft: 3,
  isDragging: false,
  isFlying: false,
  bird: {
    x: slingAnchor.x,
    y: slingAnchor.y,
    vx: 0,
    vy: 0,
    radius: baseRadius,
  },
  pigs: [],
  blocks: [],
};

function resetTargets() {
  state.pigs = [
    { x: 690, y: 310, radius: 22, hit: false },
    { x: 760, y: 280, radius: 20, hit: false },
    { x: 720, y: 250, radius: 18, hit: false },
  ];
  state.blocks = [
    { x: 670, y: 360, w: 40, h: 80, hit: false },
    { x: 720, y: 360, w: 40, h: 80, hit: false },
    { x: 750, y: 300, w: 100, h: 20, hit: false },
  ];
}

function resetBird() {
  state.bird.x = slingAnchor.x;
  state.bird.y = slingAnchor.y;
  state.bird.vx = 0;
  state.bird.vy = 0;
  state.isFlying = false;
}

function resetGame() {
  state.birdsLeft = 3;
  state.pigsLeft = 3;
  resetBird();
  resetTargets();
  updateHud();
  hintEl.textContent = "拖拽小鸟开始";
}

function updateHud() {
  birdsLeftEl.textContent = state.birdsLeft.toString();
  const pigsRemaining = state.pigs.filter((pig) => !pig.hit).length;
  state.pigsLeft = pigsRemaining;
  pigsLeftEl.textContent = pigsRemaining.toString();
}

function drawBackground() {
  ctx.fillStyle = "#7cc36c";
  ctx.fillRect(0, canvas.height * 0.62, canvas.width, canvas.height * 0.38);
  ctx.fillStyle = "#f7d76b";
  ctx.fillRect(0, canvas.height * 0.72, canvas.width, canvas.height * 0.28);

  ctx.fillStyle = "#6b4a2b";
  ctx.fillRect(40, 330, 18, 120);
  ctx.beginPath();
  ctx.arc(52, 320, 26, Math.PI, 0);
  ctx.fillStyle = "#4b2f15";
  ctx.fill();
}

function drawSling() {
  ctx.strokeStyle = "#4b2f15";
  ctx.lineWidth = 6;
  ctx.beginPath();
  ctx.moveTo(110, 420);
  ctx.lineTo(140, 300);
  ctx.lineTo(170, 420);
  ctx.stroke();

  ctx.strokeStyle = "#3a1c0f";
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.moveTo(140, 305);
  ctx.lineTo(state.bird.x, state.bird.y);
  ctx.lineTo(170, 410);
  ctx.stroke();
}

function drawBird() {
  ctx.fillStyle = "#ff5a5f";
  ctx.beginPath();
  ctx.arc(state.bird.x, state.bird.y, state.bird.radius, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#fff";
  ctx.beginPath();
  ctx.arc(state.bird.x + 6, state.bird.y - 4, 5, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#1f1f1f";
  ctx.beginPath();
  ctx.arc(state.bird.x + 7, state.bird.y - 4, 2, 0, Math.PI * 2);
  ctx.fill();
}

function drawBlocks() {
  state.blocks.forEach((block) => {
    ctx.fillStyle = block.hit ? "#b7b7b7" : "#d9a441";
    ctx.fillRect(block.x, block.y, block.w, block.h);
    ctx.strokeStyle = "#9e7330";
    ctx.strokeRect(block.x, block.y, block.w, block.h);
  });
}

function drawPigs() {
  state.pigs.forEach((pig) => {
    ctx.fillStyle = pig.hit ? "#a4a4a4" : "#6fcf5b";
    ctx.beginPath();
    ctx.arc(pig.x, pig.y, pig.radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = pig.hit ? "#666" : "#1c3d1f";
    ctx.beginPath();
    ctx.arc(pig.x - 5, pig.y - 4, 3, 0, Math.PI * 2);
    ctx.arc(pig.x + 6, pig.y - 4, 3, 0, Math.PI * 2);
    ctx.fill();
  });
}

function drawTrajectory() {
  if (!state.isDragging) return;
  ctx.strokeStyle = "rgba(255, 90, 95, 0.5)";
  ctx.lineWidth = 2;
  ctx.setLineDash([6, 6]);
  ctx.beginPath();
  ctx.moveTo(state.bird.x, state.bird.y);
  ctx.lineTo(slingAnchor.x, slingAnchor.y);
  ctx.stroke();
  ctx.setLineDash([]);
}

function updatePhysics() {
  if (!state.isFlying) return;
  state.bird.vy += gravity;
  state.bird.x += state.bird.vx;
  state.bird.y += state.bird.vy;

  if (state.bird.y + state.bird.radius > canvas.height * 0.72) {
    state.bird.y = canvas.height * 0.72 - state.bird.radius;
    state.bird.vy *= -0.4;
    state.bird.vx *= 0.85;
  }

  if (state.bird.x + state.bird.radius > canvas.width || state.bird.x - state.bird.radius < 0) {
    state.bird.vx *= -0.6;
  }

  if (Math.abs(state.bird.vx) < 0.15 && Math.abs(state.bird.vy) < 0.15) {
    state.isFlying = false;
    if (state.pigsLeft > 0 && state.birdsLeft > 0) {
      hintEl.textContent = "继续拖拽下一只小鸟";
      resetBird();
    }
  }
}

function checkCollisions() {
  state.blocks.forEach((block) => {
    if (block.hit) return;
    const closestX = Math.max(block.x, Math.min(state.bird.x, block.x + block.w));
    const closestY = Math.max(block.y, Math.min(state.bird.y, block.y + block.h));
    const dx = state.bird.x - closestX;
    const dy = state.bird.y - closestY;
    if (dx * dx + dy * dy < state.bird.radius ** 2) {
      block.hit = true;
      state.bird.vx *= 0.7;
      state.bird.vy *= -0.6;
    }
  });

  state.pigs.forEach((pig) => {
    if (pig.hit) return;
    const dx = state.bird.x - pig.x;
    const dy = state.bird.y - pig.y;
    const distance = Math.hypot(dx, dy);
    if (distance < state.bird.radius + pig.radius) {
      pig.hit = true;
      hintEl.textContent = "命中！";
    }
  });
}

function updateGameState() {
  if (state.pigsLeft === 0) {
    hintEl.textContent = "全部命中！你赢了！";
  } else if (state.birdsLeft === 0 && !state.isFlying) {
    hintEl.textContent = "小鸟用完了，点击重置再试";
  }
}

function render() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  drawBackground();
  drawSling();
  drawBlocks();
  drawPigs();
  drawTrajectory();
  drawBird();
}

function loop() {
  updatePhysics();
  checkCollisions();
  updateHud();
  updateGameState();
  render();
  requestAnimationFrame(loop);
}

function onPointerDown(event) {
  if (state.isFlying || state.birdsLeft === 0) return;
  const { offsetX, offsetY } = event;
  const distance = Math.hypot(offsetX - state.bird.x, offsetY - state.bird.y);
  if (distance <= state.bird.radius + 10) {
    state.isDragging = true;
    hintEl.textContent = "松手发射";
  }
}

function onPointerMove(event) {
  if (!state.isDragging) return;
  const { offsetX, offsetY } = event;
  const dx = offsetX - slingAnchor.x;
  const dy = offsetY - slingAnchor.y;
  const distance = Math.min(Math.hypot(dx, dy), maxPull);
  const angle = Math.atan2(dy, dx);
  state.bird.x = slingAnchor.x + Math.cos(angle) * distance;
  state.bird.y = slingAnchor.y + Math.sin(angle) * distance;
}

function onPointerUp() {
  if (!state.isDragging) return;
  state.isDragging = false;
  const dx = slingAnchor.x - state.bird.x;
  const dy = slingAnchor.y - state.bird.y;
  state.bird.vx = dx * 0.22;
  state.bird.vy = dy * 0.22;
  state.isFlying = true;
  state.birdsLeft -= 1;
  hintEl.textContent = "飞行中...";
}

canvas.addEventListener("pointerdown", onPointerDown);
canvas.addEventListener("pointermove", onPointerMove);
canvas.addEventListener("pointerup", onPointerUp);
canvas.addEventListener("pointerleave", onPointerUp);
resetButton.addEventListener("click", resetGame);

resetGame();
loop();
