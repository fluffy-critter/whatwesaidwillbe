# whatwesaidwillbe

This is an experimental thingamajig for a possible art gallery installation. It just records the audio from a stereo input, and plays it back with a time delay. For the actual installation there will be two microphones connected to a mixer that feeds the audio near each speaker (including conversations, background music, etc.) to the opposite speaker, with some gentle equalization to avoid egregious feedback effects. I also intend to have an abstract musical performance piece or two.

It's also fun to hook instruments directly up to the mixer and inject their audio into the feedback loop. Guitars and Kaossilators work especially well for this.

This requires CMake, ALSA, GLUT, and a lot of patience.

## Recommended Configuration

* Computer running Linux with a 2x2 audio interface (most laptops only have 2x1, but you can get a decent 2x2 USB interface for around $30)
** Pulseaudio configured with said 2x2 interface as the primary input and output
** Interface output going to decent speakers, separated by several feet, or ideally in different rooms
* Two condenser microphones (I use MXL V63m), each one sitting 5 feet in front of a speaker
* The microphones are connected to a mixer with phantom power, panned to the opposite of the speakers they face
* The mixer is hooked up to the audio interface inputs

## Calibration

This is a fidgety process but what I do for now is:

1. Adjust your Pulseaudio levels such that music played from the computer is at a comfortable level, and such that the microphones register a reasonable input level (at around 50% of peak)
2. Run `./whatwesaidwillbe -c 1`
3. After latency calibration completes, turn the volume down all the way
4. Have an audio source playing in the room at a reasonable volume, about the same distance from the microphone as it is from the speaker
5. Adjust the microphone levels up until the black spikes come to about halfway to the red circle
6. Stop the audio source
7. Turn up the output volume until a word spoken at a normal volume repeats for a long time without the resonance frequencies dominating (you can also adjust the EQ on the mixer to cut out extreme bass or treble, if your mixer supports it)
8. Get a grant to exhibit this in MoMA (I'm still working on that part)
