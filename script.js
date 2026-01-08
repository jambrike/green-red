const video = document.getElementById("video");
const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const statusText = document.getElementById("status");

//extra status
const hitIndicator = document.getElementById("hitIndicator");


let detector;
let redLight = false;
let referencePose = null;
let cooldown = false;

navigator.mediaDevices.getUserMedia({ video: true })
  .then(stream => {
    video.srcObject = stream;
  });


async function init() {
  detector = await poseDetection.createDetector(
  poseDetection.SupportedModels.MoveNet,
  {
    modelType: poseDetection.movenet.modelType.SINGLEPOSE_LIGHTNING
  }
);

  requestAnimationFrame(loop);
}

init();

// --- MAIN LOOP ---
async function loop() {
  if (video.readyState === 4) {
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;

    ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

    const poses = await detector.estimatePoses(video);

    if (poses.length > 0) {
      drawPose(poses[0].keypoints);

      if (redLight) {
        checkMovement(poses[0].keypoints);
      }
    }
  }

  requestAnimationFrame(loop);
}

// --- DRAW SKELETON --
function drawPose(keypoints) {
  ctx.fillStyle = "red";

  keypoints.forEach(p => {
    if (p.score > 0.4) {
      ctx.beginPath();
      ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
      ctx.fill();
    }
  });
}

// --- MOVEMENT DETECTION ---
function checkMovement(current) {
  if (!referencePose) {
    referencePose = current.map(p => ({ x: p.x, y: p.y }));
    return;
  }

  let movement = 0;

  for (let i = 0; i < current.length; i++) {
    const dx = current[i].x - referencePose[i].x;
    const dy = current[i].y - referencePose[i].y;
    movement += Math.sqrt(dx * dx + dy * dy);
  }

  if (movement > 260 && !cooldown) {
    triggerHit();
  }
}

function triggerHit() {
  cooldown = true;

  console.log("❌ MOVED DURING RED");

  
  hitIndicator.style.display = "block";

  console.log("🔥 HIT");


  setTimeout(() => {
    hitIndicator.style.display = "none";
    cooldown = false;
    referencePose = null;
  }, 1500);
}

function setRed(state) {
  redLight = state;
  referencePose = null;
  statusText.textContent = state ? "RED" : "GREEN";
}

setInterval(() => {
  setRed(!redLight);
}, 4000);
