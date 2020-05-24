function VideoUtils() {
    let self = this;

    function resizeCanvas() {
        self.canvas.style.width = window.innerWidth + "px";
        self.canvas.style.height = window.innerHeight + "px";
        self.canvas.width = window.innerWidth;
        self.canvas.height = window.innerHeight;
        // compute scale
        self.scale = Math.min(
            self.canvas.width / video.videoWidth, 
            self.canvas.height / video.videoHeight);    
        
        if (self.onCameraResizeCallback) self.onCameraResizeCallback();
    }

    function onVideoCanPlay() {
        self.videoReady = true;
        // get canvas
        self.canvas = document.getElementById(self.videoCanvasId); // get the canvas from the page
        // set fullscreen
        resizeCanvas();
        window.onresize = resizeCanvas; // call again on window resize
        // get draw context 
        self.ctx = self.canvas.getContext("2d");
        if (self.onCameraStartedCallback) {
            self.onCameraStartedCallback(self.canvas);
        }
        requestAnimationFrame(updateCanvas);
    };

    function updateCanvas(){
        self.ctx.clearRect(0,0,self.canvas.width,self.canvas.height); 
        // only draw if loaded and ready
        if(self.video !== undefined && self.videoReady){ 
            // find the top left of the video on the canvas
            var scale = self.scale;
            var vidH = self.video.videoHeight;
            var vidW = self.video.videoWidth;
            var top = self.canvas.height / 2 - (vidH /2 ) * scale;
            var left = self.canvas.width / 2 - (vidW /2 ) * scale;
            // now just draw the video the correct size
            self.ctx.drawImage(self.video, left, top, vidW * scale, vidH * scale);
        }
        // request the next frame 
        requestAnimationFrame(updateCanvas);
    }

    this.startCamera = function(startCallback, resizeCallback, videoCanvasId) {
        video = document.createElement('video');

        let videoConstraint = { 
            video: {
                width: { max: 720 },
                height: { max: 480 } 
            } 
        };

        if (!videoConstraint) {
            videoConstraint = true;
        }

        video.autoPlay = false; // ensure that the video does not auto play
        self.videoReady = false;
        self.videoCanvasId = videoCanvasId;

        navigator.mediaDevices.getUserMedia({video: videoConstraint, audio: false})
            .then(function(stream) {
                video.srcObject = stream;
                video.play();
                self.video = video;
                self.stream = stream;
                self.onCameraStartedCallback = startCallback;
                self.onCameraResizeCallback = resizeCallback;
                video.addEventListener('canplay', onVideoCanPlay, false);
            })
            .catch(function(err) {
                console.log('Camera Error: ' + err.name + ' ' + err.message);
            });
    };

    this.stopCamera = function() {
        if (this.video) {
            this.video.pause();
            this.video.srcObject = null;
            this.video.removeEventListener('canplay', onVideoCanPlay);
        }
        if (this.stream) {
            this.stream.getVideoTracks()[0].stop();
        }
    };
    
};