// --- UI Update Functions ---

function updateSystemState(data) {
    document.getElementById('sysRole').textContent = data.role;
    document.getElementById('sysState').textContent = data.state;
    
    const killedEl = document.getElementById('sysKilled');
    killedEl.textContent = data.killed ? "TRUE" : "FALSE";
    killedEl.style.color = data.killed ? "var(--alert-glow)" : "var(--success-glow)";
}

function updateCompass(heading, gyroData) {
    // CSS rotation for compass
    const arrow = document.getElementById('compassArrow');
    const val = document.getElementById('compassValue');
    arrow.style.transform = `rotate(${heading}deg)`;
    val.textContent = `${Math.round(heading)}°`;
    
    // Update IMU angles
    if (gyroData) {
        document.getElementById('imuPitch').textContent = `${gyroData.pitch.toFixed(1)}°`;
        document.getElementById('imuRoll').textContent = `${gyroData.roll.toFixed(1)}°`;
        document.getElementById('imuYaw').textContent = `${gyroData.yaw.toFixed(1)}°`;
    }
}

function updateColorSensors(sensors) {
    const map = {0: 'csFront', 1: 'csRight', 2: 'csBack', 3: 'csLeft'};
    sensors.forEach((s, idx) => {
        const el = document.getElementById(map[idx]);
        if(el) {
            el.style.setProperty('--sensor-color', `rgb(${s.r}, ${s.g}, ${s.b})`);
            el.querySelector('.cs-rgb').textContent = `${s.r},${s.g},${s.b}`;
        }
    });
}

function drawIRRadar(data) {
    const canvas = document.getElementById('irRadar');
    const ctx = canvas.getContext('2d');
    const cx = canvas.width / 2;
    const cy = canvas.height / 2;
    const radius = 100;

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw background circles
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
    ctx.lineWidth = 1;
    for(let i=1; i<=3; i++) {
        ctx.beginPath();
        ctx.arc(cx, cy, radius * (i/3), 0, Math.PI * 2);
        ctx.stroke();
    }

    // Draw sensor values (Assuming 7 sensors arranged in a semi-circle or full circle)
    const numSensors = data.raw.length;
    const angleStep = (Math.PI * 2) / numSensors;

    ctx.fillStyle = 'rgba(56, 189, 248, 0.5)';
    ctx.beginPath();
    ctx.moveTo(cx, cy);

    for(let i=0; i<numSensors; i++) {
        // VS1838B are active LOW: 0 means ball detected, 1 means no ball.
        const isActive = data.raw[i] === 0;
        const val = isActive ? 1.0 : 0.05; // Full length if detected, tiny if not
        
        const a = (i * angleStep) - Math.PI/2; // Start at top
        
        const x = cx + Math.cos(a) * (radius * val);
        const y = cy + Math.sin(a) * (radius * val);
        
        ctx.lineTo(x, y);
        
        // Draw point
        ctx.save();
        ctx.fillStyle = isActive ? '#ef4444' : '#38bdf8';
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI*2);
        ctx.fill();
        ctx.restore();
    }
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = '#38bdf8';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Draw calculated ball vector
    if(data.detected) {
        const a = (data.angle * Math.PI / 180) - Math.PI/2;
        const vLen = radius * (data.intensity / 255.0);
        
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(cx + Math.cos(a) * vLen, cy + Math.sin(a) * vLen);
        ctx.strokeStyle = '#ef4444'; // Red line for ball
        ctx.lineWidth = 3;
        ctx.stroke();
    }

    // Update text
    document.getElementById('irAngle').textContent = `∠ ${data.angle}°`;
    document.getElementById('irIntensity').textContent = `⚡ ${data.intensity}`;
}

// --- Real Data Fetching (REST API Polling) ---
const ROLES = { 0: "ATTACKER", 1: "DEFENDER" };
const STATES = { 
    0: "IDLE", 1: "SEARCH", 2: "APPROACH", 3: "DRIBBLE", 
    4: "SHOOT", 5: "DEFEND", 6: "REPOSITION", 7: "AVOID_PENALTY" 
};

let errorCount = 0;

async function fetchTelemetry() {
    try {
        const response = await fetch('/api/telemetry');
        if (!response.ok) throw new Error("HTTP Error");
        
        const data = await response.json();
        
        // Update connection status
        errorCount = 0;
        document.getElementById('connStatus').className = 'status-badge';
        document.getElementById('connText').textContent = 'Connected';

        // 1. System State
        const roleStr = ROLES[data.core.role] || "UNKNOWN";
        const stateStr = STATES[data.core.state] || "UNKNOWN";
        
        updateSystemState({
            role: roleStr,
            state: stateStr,
            killed: data.core.killed
        });

        // 2. Heading + IMU
        const gyroData = data.gyro ? {
            pitch: data.gyro.pitch,
            roll: data.gyro.roll,
            yaw: data.gyro.yaw
        } : null;
        updateCompass(data.core.heading, gyroData);

        // 3. IR Radar & Individual Sensors
        const irRawArray = [
            data.ir.s0, data.ir.s1, data.ir.s2, data.ir.s3, 
            data.ir.s4, data.ir.s5, data.ir.s6
        ];
        
        drawIRRadar({
            detected: data.core.ball_conf > 0, // Using confidence as detection flag
            angle: data.core.ball_angle,
            intensity: data.core.ball_conf,
            raw: irRawArray
        });

        // Update individual IR badges
        for (let i = 0; i < 7; i++) {
            const span = document.getElementById(`irRaw${i}`);
            if (span) span.textContent = irRawArray[i];
        }

        // 4. Color Sensors
        // The backend sends data.color as an object: {"s0":{...}, "s1":{...}, ...}
        const colorData = data.color ? [
            data.color.s0 || {r: 0, g: 0, b: 0},
            data.color.s1 || {r: 0, g: 0, b: 0},
            data.color.s2 || {r: 0, g: 0, b: 0},
            data.color.s3 || {r: 0, g: 0, b: 0}
        ] : [
            {r: 0, g: 0, b: 0},
            {r: 0, g: 0, b: 0},
            {r: 0, g: 0, b: 0},
            {r: 0, g: 0, b: 0}
        ];
        updateColorSensors(colorData);

        // 5. Uptime
        const uptimeSecs = Math.floor(data.uptime_ms / 1000);
        const h = Math.floor(uptimeSecs / 3600).toString().padStart(2, '0');
        const m = Math.floor((uptimeSecs % 3600) / 60).toString().padStart(2, '0');
        const s = (uptimeSecs % 60).toString().padStart(2, '0');
        document.getElementById('sysUptime').textContent = `${h}:${m}:${s}`;

    } catch (err) {
        errorCount++;
        if (errorCount > 3) {
            document.getElementById('connStatus').className = 'status-badge disconnected';
            document.getElementById('connText').textContent = 'Disconnected';
        }
    }
}

// Poll every 100ms (10Hz)
setInterval(fetchTelemetry, 100);
