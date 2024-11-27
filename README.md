# Metronome
A basic metronome usable from command line.

Only made for Windows for now.

## Build setup
Should install cmake and MSVC build tools at least for 2019.

Most likely easiest to simply use Visual Studio to build, I've complicated things for myself by simply working in the command line.

## Usage
I most likely expect problems on other machines as I've not tested or accounted for other audio devices/formats.

If no problems occur then ideally:
- After building, simply run the executable.
    - Sine beeps should start playing
    - Default values:
        - BPM = 120
        - Time Signature = 4/4 (Time signature will always be over 4 as of now)
        - Freq of tone = 49th piano key (Tuned for 440hz)

Optional Arguments (Order must be as listed):
1. BPM: The bpm for metronome.
2. Time Signature: Time signature on which to play alternative frequency, this value will automatically be over 4 (e.g. If '3' is entered, the signature will be 3/4)
3. Piano Note: Ultimately the frequency, denoted by which number key on the piano (The downbeat of the time signature will have frequency of `note + 8`)
