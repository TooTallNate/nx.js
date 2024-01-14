console.log('hi');

let audioCtx, myArrayBuffer, nowBuffering, source;

function play() {
	audioCtx = new AudioContext();
	console.log(audioCtx);

	// Create an empty three-second stereo buffer at the sample rate of the AudioContext
	myArrayBuffer = audioCtx.createBuffer(
		2,
		audioCtx.sampleRate * 3,
		audioCtx.sampleRate
	);

	// Fill the buffer with white noise;
	// just random values between -1.0 and 1.0
	nowBuffering;
	for (let channel = 0; channel < myArrayBuffer.numberOfChannels; channel++) {
		// This gives us the actual array that contains the data
		nowBuffering = myArrayBuffer.getChannelData(channel);
		for (let i = 0; i < myArrayBuffer.length; i++) {
			// Math.random() is in [0; 1.0]
			// audio needs to be in [-1.0; 1.0]
			nowBuffering[i] = Math.random() * 2 - 1;
		}
		console.log(nowBuffering);
	}

	// Get an AudioBufferSourceNode.
	// This is the AudioNode to use when we want to play an AudioBuffer
	source = audioCtx.createBufferSource();
	source.onended = console.log;

	// set the buffer in the AudioBufferSourceNode
	source.buffer = myArrayBuffer;

	// connect the AudioBufferSourceNode to the
	// destination so we can hear the sound
	source.connect(audioCtx.destination);

	// start the source playing
	source.start();

	console.log(nowBuffering);
}

if (typeof Switch !== 'undefined') {
	play();
} else {
	document.body.onclick = play;
}
