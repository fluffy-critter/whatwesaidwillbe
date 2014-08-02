# whatwesaidwillbe #

This is an experimental thingamajig for a possible art gallery installation. It just records the audio from a stereo input, and plays it back with a time delay. For the actual installation there will be two microphones connected to a mixer that feeds the audio near each speaker (including conversations, background music, etc.) to the opposite speaker, with some gentle equalization to avoid egregious feedback effects. I also intend to have an abstract musical performance piece or two.

It's also fun to hook instruments directly up to the mixer and inject their audio into the feedback loop. Guitars and Kaossilators work especially well for this.

This requires CMake, ALSA, GLUT, boost, and a lot of patience.

## Recommended Configuration ##

* Computer running Linux with a 2x2 audio interface (most laptops only have 2x1, but you can get a decent 2x2 USB interface for around $30)
  * Pulseaudio configured with said 2x2 interface as the primary input and output
  * Interface output going to decent speakers, separated by several feet, or ideally in different rooms
* Two condenser microphones (I use MXL V63m), each one sitting 5 feet in front of a speaker
  * The microphones are connected to a mixer with phantom power, panned to the opposite of the speakers they face
  * The mixer is hooked up to the audio interface inputs

## Calibration ##

This is a fidgety process but what I do for now is:

1. Adjust your Pulseaudio levels such that music played from the computer is at a comfortable level, and such that the microphones register a reasonable input level (at around 50% of peak)
2. Run `./whatwesaidwillbe -c 1`
3. After latency calibration completes, turn the volume down all the way
4. Have an audio source playing in the room at a reasonable volume, about the same distance from the microphone as it is from the speaker
5. Adjust the microphone levels up until the black spikes come to about halfway to the red circle (you'll probably need to clap once or twice to make the circle visible)
6. Stop the audio source
7. Turn up the output volume until a word spoken at a normal volume repeats for a while without the resonance frequencies dominating (you can also adjust the EQ on the mixer to cut out extreme bass or treble, if your mixer supports it)
8. Press Esc to exit, then run `./whatwesaidwillbe` on its own (or with whatever other settings you want to play with; see `./whatwesaidwillbe --help` for more)
8. Get a grant to exhibit this in MoMA (I'm still working on that part)

## User interface

* `Esc`: quit
* letter keys: set a parameter; or shows a list if unknown key (`?` is always safe for that)
* up/down: adjust the current parameter, if any

## Startup Options

### Configurations
* `--list-devices`: List the known ALSA devices. Somehow this can be used for `--capture` and `--playback` but I just use pulseaudio anyway.
* `--rate`/`-r`: The sample rate. I do all my testing at 44100. If your interface natively supports 48000 or higher, feel free to try it.
* `--bufSize`/`-k`: The processing buffer size, in samples. This affects a bunch of stuff.
* `--historySize`/`-H`: The history buffer size. Only affects the quality of the visualization.
* `--loopDelay`/`-c`: How long between repeats of audio.
* `--latency`/`-q`: How much latency to request from ALSA. If the audio stutters, try raising this.
* `--capture`: The ALSA device to record from. I just use pulseaudio.
* `--playback`: `$_ ~= s/record from/play back to/`
* `--recDump`: Record the audio inputs to a raw PCM file. You can use sox to convert this to a wav (`sox -t raw -b 16 -e signed-integer -r 44100 -c2 -X`)
* `--playDump`: Record the playback buffer to a raw PCM file. Pretty much just `--recDump` but delayed and with the volume level changes applied.

### Knobs

* `--dampen`/`-d`: How much to dampen volume adjustment by. 0 means instantly go to the target volume, 1 means never change the volume at all. This is applied on a per-buffer basis.
* `--feedThresh`/`-F`: The lower recording level to start applying the feedback model to
* `--limiter`/`-L`: The highest recording level (as measured by the microphones) that you want to ever hear come out of your speakers (.5 is roughly white noise played at full blast). This level is indicated by the red circle around the visualization.
* `--mode`/`-m`: Which volume control mode to use
* `--feedback`/`-f`: The attempted ratio for the feedback model. 1.0 means that the feedback model will try to always have the output always get recorded at the same level.
* `--target`/`-t`: The target recorded power level; make quieter stuff louder, make louder stuff quieter.
* `--gain`/`-g`: Simple gain change. Mostly equivalent to changing the volume knob on your speakers, except that it is subject to clipping limits (so if the signal is already peaking, this can't make it go any higher).

