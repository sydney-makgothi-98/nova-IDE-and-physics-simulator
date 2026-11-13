@@part Part IV — The Recorder and the Tests
# The Direct-to-Video Recorder

The Record button turns what you see into a normal video file you can play, share and put in a presentation. This chapter explains how video works, how Windows' built-in encoder is driven, and every line of <code>encoder.hpp</code>.

## What a video really is

A video is a **flipbook**: a list of still pictures (called **frames**) shown quickly one after another. Show 60 pictures every second and the eye sees smooth motion.

Three numbers describe it:

* the **resolution**: how many pixels wide and tall each frame is (our videos are 1020 × 800 with the standard window size);
* the **frame rate**: how many frames per second (we use 60);
* the **duration**: how long it plays.

### Why compression is unavoidable

Each of our pixels takes 4 bytes. One frame is 1020 × 800 × 4 = 3.26 million bytes. At 60 frames per second that is **196 million bytes every second**: one minute of raw video would be almost 12 gigabytes. Nobody could save or share that.

A **codec** (**co**der-**dec**oder) is a method for squeezing video. The one we use is **H.264** (also called AVC), the most widely supported codec in the world. It exploits two facts:

1. **Neighbouring pixels are similar** (a flat dark background). Store the *difference* instead of every pixel.
2. **Consecutive frames are similar** (only the masses and curves moved). Store only *what changed* since the last frame.

Our test video: 6 seconds, 360 frames, **5.0 million bytes**: about 0.8 million bytes per second. That is a squeeze of more than 200 to 1.

::: def
**Codec.** The compression method (H.264).

**Container.** The file format that holds the compressed pictures plus an index (MP4). People often say "MP4" for both; they are different things: the container is the box, the codec is how the contents were packed.

**Bitrate.** How many bits per second the compressed video uses. We ask for 8 million (8 Mbit/s, about 1 MB/s); simple content like ours needs less, and the encoder uses what it needs.
:::

{{fig:gl-mp4|An MP4 file is a series of labelled "boxes". The index (moov) is written at the very end, which is why the encoder must be finalised.}}

## Why we did not write our own encoder, and did not use ffmpeg

Writing an H.264 encoder from scratch takes a large team and years. The popular tool **ffmpeg** is excellent, but it is a separate download that every user would have to install, and we wanted the program to work on a plain Windows PC. Windows already contains an H.264 encoder as part of **Media Foundation**, its multimedia framework. We use that. The cost: Windows only. The gain: zero installation.

## A very short COM primer

Media Foundation is built with **COM** (Component Object Model), an older Microsoft technology. You only need four ideas:

* An **interface** is a set of functions on an object you cannot see inside. Our code holds a **pointer to an interface**, such as <code>IMFSinkWriter*</code>.
* COM objects **count how many users they have**. When you are finished you must call <code>Release()</code>, which lowers the count; at zero the object destroys itself. Forgetting <code>Release()</code> is a *memory leak*.
* Almost every COM function returns an **HRESULT**, a number where success is non-negative and failure is negative. The macros <code>SUCCEEDED(hr)</code> and <code>FAILED(hr)</code> test it. **Always check.**
* A program must **initialise** COM (<code>CoInitializeEx</code>) and Media Foundation (<code>MFStartup</code>) once before using them.

## The Sink Writer pipeline

The Media Foundation object we use is the **Sink Writer**. You give it *uncompressed* pictures; it finds an encoder, compresses them and writes the MP4 file.

{{fig:gl-recpipe|From a drawn frame to a finished MP4 file. Our code does the first three boxes; Windows does the rest.}}

Giving a picture to the Sink Writer takes four steps: describe **what you will send in**, describe **what you want stored**, say **begin**, then send frames one at a time and say **finalize**.

## encoder.hpp: the header comment and includes

@@src encoder.hpp | ^// encoder.hpp | ^#include <vector>

* The comment shows the whole interface in six lines of example code. A good header tells you how to *use* it before you read how it works.
* <code>mfapi.h, mfidl.h, mfreadwrite.h, mferror.h</code> declare the Media Foundation functions and types.
* The matching libraries (<code>mfplat</code>, <code>mfreadwrite</code>, <code>mfuuid</code>, <code>ole32</code>) are listed in <code>CMakeLists.txt</code>. <code>mfuuid</code> supplies the long identifier numbers (GUIDs) such as <code>MFVideoFormat_H264</code>; forgetting it gives "undefined reference" errors at link time.

## The class and its small accessors

@@src encoder.hpp | ^class VideoEncoder \{ | +6

* **<code>~VideoEncoder() { if (writer_) close(); }</code>** The **destructor**: if the encoder object disappears while a file is still open, it finishes the file automatically. This is RAII again: cleanup is guaranteed even if the program is shutting down in a hurry.
* <code>isOpen()</code> tells whether a recording is in progress; <code>framesWritten()</code> and <code>seconds()</code> report progress (frames divided by the frame rate).

## open: set everything up

@@src encoder.hpp | ^    bool open\( | ^    \}$

Walk through it in the order the pipeline needs.

**Checks first.**

* <code>if (writer_) { err = "already recording"; return false; }</code> One recording at a time.
* <code>if (width &lt; 16 || ... || (width &amp; 1) || (height &amp; 1))</code> The size must be **even** and at least 16. H.264 stores colour at half resolution (pairs of pixels share a colour: *4:2:0 subsampling*), and most encoders refuse odd sizes. <code>(width &amp; 1)</code> tests the lowest bit: 1 means odd. The caller makes the size even with <code>(W - PANEL_W) &amp; ~1</code>, which clears that bit.
* <code>startup(err)</code> starts COM and Media Foundation (once).

**Creating the writer.**

* <code>MultiByteToWideChar(CP_UTF8, ...)</code> converts the file name to wide characters, as Windows wants.
* <code>MFCreateAttributes(&amp;attr, 2)</code> makes a small settings bag with room for two.
* <code>MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS = TRUE</code>: "use the graphics card's video encoder if there is one", which is faster and frees the CPU. If there is none, the software encoder is used automatically.
* <code>MF_SINK_WRITER_DISABLE_THROTTLING = TRUE</code>: "never drop frames to keep up". For a recording we want *every* frame, even if encoding is slow.
* <code>MFCreateSinkWriterFromURL(wpath, nullptr, attr, &amp;writer_)</code> creates the writer and the (empty) file. It chooses the container from the extension <code>.mp4</code>. We then <code>Release()</code> the settings bag, which we no longer need, and test the result with <code>FAILED(hr)</code>.

**What to store (the output type).**

* <code>MFCreateMediaType(&amp;out)</code> makes a media-type description. We set: major type = video; subtype = <code>MFVideoFormat_H264</code>; average bitrate; progressive (not interlaced); the frame size; the frame rate as the fraction <code>fps / 1</code>; and a pixel aspect ratio of <code>1 / 1</code> (square pixels).
* The frame size and rate are *pairs* of numbers. Media Foundation stores a pair in one 64-bit number with one value in the top half and the other in the bottom half. Our helper <code>pack(hi, lo)</code> does <code>((UINT64)hi &lt;&lt; 32) | lo</code>.
* <code>writer_-&gt;AddStream(out, &amp;stream_)</code> tells the writer "there will be a video stream with this format" and returns the stream's number. The type object is released afterwards.

**What we will feed it (the input type).**

* The same kind of description, but the subtype is <code>MFVideoFormat_RGB32</code>: plain 32-bit pixels, which is what our canvas holds. <code>SetInputMediaType</code> says "we will send you these". The writer looks for a converter and an encoder that bridge RGB32 to H.264. If none exists, this call fails: the reason we can report "encoder rejected the picture format".

**Start.**

* <code>BeginWriting()</code> opens the pipeline. Finally the size, rate and frame counter are remembered (<code>w_, h_, fps_, frame_ = 0</code>).

::: why
**Why describe the input and output separately?** Because they are different things. What *you* have (raw pixels) is not what the *file* wants (compressed H.264). The Sink Writer sits between the two and inserts whatever conversion and encoding is needed. If you later wanted H.265 or a different container, only the output description would change.
:::

## addFrame: send one picture

@@src encoder.hpp | ^    // pixels: top-down 32-bit | ^    \}$

* <code>const DWORD bytes = w_ * h_ * 4;</code> The size of one frame in bytes.
* <code>MFCreateMemoryBuffer(bytes, &amp;buf)</code> allocates a block of memory that Media Foundation owns. <code>buf-&gt;Lock(&amp;dst, ...)</code> gives us a pointer to it so we can write into it; <code>Unlock</code> hands it back.
* **The row flip.** The loop <code>for (int y = 0; y &lt; h_; ++y) memcpy(dst + (h_ - 1 - y) * w_ * 4, src + y * rowBytes, w_ * 4);</code> copies the picture row by row, **top row of our canvas to the bottom of the buffer**. Our canvas stores rows top to bottom; Media Foundation expects uncompressed RGB rows **bottom to top** (a tradition from the old BMP file format). Without the flip every video would be upside down.
* <code>(size_t)y * rowBytes</code> uses <code>rowBytes</code> (the canvas's row length) for the source, so the encoder can copy a **left part** of a wider canvas: our video leaves out the control panel by copying only the first <code>w_</code> pixels of each row.
* <code>buf-&gt;SetCurrentLength(bytes)</code> says how much of the buffer is valid.
* **The sample.** <code>MFCreateSample(&amp;sample)</code> makes a container for "one unit of media data with a time stamp"; <code>AddBuffer</code> puts our pixels in it.
* **The times.** <code>sample-&gt;SetSampleTime(frame_ * 10000000LL / fps_);</code> Media Foundation measures time in units of **100 nanoseconds**, so one second is 10 000 000. The time of frame *n* at 60 fps is <code>n * 10000000 / 60</code>. We compute it **from the frame number** each time rather than adding 1/60 repeatedly: repeated addition of a rounded value would *drift*, whereas multiplying fresh from the integer frame number is exact to within one unit. <code>SetSampleDuration(10000000LL / fps_)</code> says how long the frame is shown.
* <code>writer_-&gt;WriteSample(stream_, sample)</code> sends it to the encoder. Then **both the sample and the buffer are released**; the writer keeps what it needs. Releasing a frame's worth of memory 60 times a second is essential: forgetting would leak about 200 MB per second.
* <code>++frame_;</code> counts the frame only after success.

## close: finish the file

@@src encoder.hpp | ^    bool close | ^    \}$

* <code>writer_-&gt;Finalize()</code> flushes any frames still inside the encoder and writes the index (the <code>moov</code> box). **Without this call the MP4 is unplayable**, because players need the index to find the frames. <code>Release()</code> then frees the writer; the pointer is set to <code>nullptr</code> so a second <code>close()</code> does nothing harmful.

::: whatif
**What if the program crashes, or the power fails, while recording?** The index is never written, and the file will not open. That is a real weakness of the MP4 format. A safeguard, if you want one, is to record in short **segments** (say, 30 seconds each) so a crash loses at most one segment. The program does handle the *polite* ways of ending: Stop Record, closing the window (<code>WM_DESTROY</code> calls <code>stopRecording</code>), and the destructor.
:::

## The private part: helpers and cleanup

@@src encoder.hpp | ^private: | ^    static UINT64 pack
@@src encoder.hpp | ^    static bool startup | ^    \}$
@@src encoder.hpp | ^    static std::string hrText | ^    \}$
@@src encoder.hpp | ^    bool fail | ^    \}$

* The private fields: the writer pointer <code>writer_</code>, the stream number, size, rate and frame counter. A trailing underscore (<code>writer_</code>) is a naming habit for "this is a member variable".
* **<code>startup</code>** uses <code>static bool done, ok</code>: variables that survive between calls, so the expensive start-up runs **once** however many recordings you make. <code>CoInitializeEx(nullptr, COINIT_MULTITHREADED)</code> starts COM; <code>MFStartup(MF_VERSION)</code> starts Media Foundation and checks that the library version matches the headers.
* **<code>hrText</code>** turns a failure code into readable text: <code>"encoder rejected the picture format (error 0x...)"</code>. The hexadecimal code can be looked up if the message is not enough.
* **<code>fail</code>** is the single place that cleans up after a failed <code>open</code>: it builds the error text and releases the half-built writer, so every error path in <code>open</code> is one line: <code>return fail("...", hr, err);</code>.

## Connecting the recorder to the program (live.cpp)

@@src live.cpp | ^static VideoEncoder encoder | ^static const int REC_FPS

* One global <code>encoder</code> object, the recording size (<code>recW, recH</code>), and <code>REC_FPS = 60</code>.

@@src live.cpp | ^static void startRecording | ^\}$

* **The size.** <code>recW = (W - PANEL_W) &amp; ~1; recH = H &amp; ~1;</code> The video holds the scene and plots **without the control panel**, cut to an even size. The size is fixed *at the moment recording starts*: if you resize the window later, the video size cannot change.
* The path is <code>recordings/vibration_YYYYMMDD_HHMMSS.mp4</code>. <code>encoder.open(...)</code> either starts the encoder or returns the error text, which becomes a toast: "Cannot record: ...".
* On success, set <code>A.recording = true</code>, reset the frame counter, store the path, and tell the user how to stop.

@@src live.cpp | ^static void stopRecording | ^\}$
@@src live.cpp | ^void toggleRecording | +1

* <code>stopRecording</code> clears the flag and calls <code>encoder.close()</code>, then reports where the file went (the toast shows only the file name, found by cutting the path after its last <code>/</code>) or that finalising failed. It does nothing if no recording is active, so it is safe to call from <code>WM_DESTROY</code> unconditionally.
* <code>toggleRecording</code> is the function declared in <code>app.hpp</code>: the Record button and F9 call it.

@@src live.cpp | ^// called once per drawn frame | ^\}$

* **Safety check:** if the canvas became smaller than the video (the user shrank the window), stop, because copying <code>recW × recH</code> pixels from a smaller buffer would read memory that is not ours.
* <code>encoder.addFrame(canvas.bits, canvas.w * 4)</code> passes the canvas pixels and the length of a row in bytes (<code>w * 4</code>). On error: stop and say so.
* <code>++A.recFrames</code> counts the frame; the REC badge converts it to minutes and seconds.

## The recording clock: why the video has the right speed

{{fig:gl-recclock|When not recording, the physics advances by the real time between frames. While recording, every frame advances exactly 1/60 s, and gets the time stamp frame × (1/60 s).}}

This is the most important design decision in the recorder. Think about what could go wrong. Suppose drawing a frame and encoding it takes 25 ms on a slow computer, and we advanced the physics by the *real* elapsed time. Then each video frame would show 25 ms of motion but be *played* for 16.7 ms: the video would run too fast.

Our solution is in the main loop: <code>advanceSim(A.recording ? 1.0 / REC_FPS : dt)</code>. While recording, **every frame advances the physics by exactly 1/60 of a second**, and the encoder stamps it as frame number *n* at *n*/60 s. The video is therefore an exact record of the simulation, no matter how slow the computer was while making it. The loop then **paces** itself to 60 frames per second (it sleeps if a frame took less than 16.7 ms) so that the app still *feels* real-time to you while you drag things.

::: whatif
**What if we recorded with the real elapsed time and variable time stamps?** The video would be correct in principle but jerky: many players struggle with variable frame rates, and the physics would take uneven steps. Fixed steps give smooth, repeatable video.
:::

## How we verified the recorder

A recorder that produces a file is not enough; the file must be *correct*. We checked in four ways:

1. **Orientation and colour test.** We wrote 90 test frames in which the top 40 rows are a green band and a red square moves to the right. We decoded the file with a video player (a Chromium <code>&lt;video&gt;</code> element), grabbed the frame at 0.5 s and looked at it: the green band was at the **top** (not flipped), the colours were right (not swapped), and the square was at the right place for frame 30. That test is what proved the row-flip loop correct.
2. **Real recording.** Using the headless mode, <code>vibration_live --demo out.mp4 --preset 3 --run 6</code> recorded 6 seconds of the real interface: 360 frames, 1020 × 800, 5.0 million bytes, a duration of 5.999983 s. Frames at 0.2 s, 1.0 s, 2.5 s, 3.9 s and 5.5 s all showed the real picture (the REC badge, the telemetry and the plots).
3. **The built-in self-test.** Check 8 of the self-test writes 30 frames and requires the file to exist and be larger than 1000 bytes.
4. **Test the tests.** In our first attempt to inspect some frames, some came out blank. The *video* was fine; our *checker* had grabbed the picture before the player had finished drawing it. After adding a short wait, every frame decoded. The lesson: when a test fails, suspect the test as well as the code.

## Chapter checks

::: quiz
1. Why is raw video too big to store? Give the calculation.
2. What is the difference between a codec and a container?
3. Why must the video width and height be even?
4. What does <code>Release()</code> do, and what happens if you forget it?
5. Why does <code>addFrame</code> flip the rows?
6. Why are frame times computed from the frame number?
7. What does <code>Finalize()</code> do, and why does a crash while recording lose the file?
8. Why does the recording advance the physics by exactly 1/60 s per frame?
:::

::: answers
1. 1020 × 800 pixels × 4 bytes × 60 frames per second ≈ 196 MB per second, nearly 12 GB per minute.
2. The codec compresses the pictures (H.264); the container (MP4) holds the compressed data plus an index.
3. H.264 stores colour at half resolution (4:2:0), which needs even dimensions.
4. It lowers a COM object's user count; at zero the object frees itself. Forgetting it leaks memory.
5. Our canvas stores the top row first; Media Foundation wants uncompressed RGB with the bottom row first.
6. Adding 1/60 repeatedly would accumulate rounding error (drift); multiplying the integer frame number is exact.
7. It flushes the encoder and writes the index. Without the index players cannot find any frames.
8. So the video's speed is exactly right however slowly the computer produced the frames.
:::

# Testing: How We Know It Is Right

Anyone can write a program that produces numbers. The real question is: *are the numbers true?* This chapter shows how the Vibration Lab proves itself with **nine automatic checks** that run in a fraction of a minute, without opening a window.

{{fig:gl-tests|The nine checks, in the order they run.}}

## Running the tests

```text
vibration_live.exe --selftest
```

The program runs the checks, prints one line per check marked <code>[PASS]</code> or <code>[FAIL]</code>, prints a summary, and **exits with code 0 if everything passed or 1 if anything failed**. A script or a build system can therefore use it directly.

## The test machinery

@@src live.cpp | ^static int gFail | ^\}$

* <code>gFail</code> counts failures. <code>check(name, ok, detail)</code> prints the line (<code>[PASS]</code> or <code>[FAIL]</code>, the name, and a detail string with the measured number) and increments <code>gFail</code> if <code>ok</code> is false. All nine checks go through this one function, so the output is uniform.

## Check 1 and 2: one mass against the textbook

@@src live.cpp | ^    \{   // 1\) single mass | ^    \}$

* Loads example 1, makes a state and a meter, and a zero <code>ext</code> list (no mouse force).
* The constants <code>m, k, c</code> are the example's; <code>wn, zeta, wd</code> are computed from equation (3.16).
* The loop advances **10 000 steps of 1 ms** (10 seconds). At every step it computes the exact answer from formula (3.17): <code>u = e^(-ζωn t) (cos ωd t + ζωn/ωd sin ωd t)</code>, for x₀ = 1 and v₀ = 0. The mass is at <code>-1 + u</code> because its equilibrium is 1 m to the left of where it started (rest length 2 m, equilibrium position −1, release at 0). It keeps the **worst gap** between exact and simulated.
* The result: **worst gap 6.91 × 10⁻¹² m**. The required bound is 10⁻⁶ m, a million times looser; we are far inside it.
* Check 2 uses the same run: the meter's residual must be below 10⁻⁴ J. We measured **3.70 × 10⁻⁹ J**.

::: why
**Why test against a formula and an energy law?** They catch different bugs. A wrong sign in a spring force would break both. A missing damper term might keep the shape right but fail the energy law; a wrong time-step bookkeeping might pass the formula but break the energy law. Using two independent yardsticks makes it very unlikely that a bug slips through both.
:::

## Check 3: a driven pair with a wiggling mouse

@@src live.cpp | ^    \{   // 2\) two masses, driven | ^    \}$

* Takes example 2, adds a drive force on mass 1 (<code>driveF = 6</code>, <code>driveW = 5</code>), *and* every step applies an extra, time-varying force on the second mass in x, <code>2.0 * sin(0.002 * i)</code>, imitating a mouse.
* Over 20 seconds it records the worst energy-law error. Result: **1.01 × 10⁻⁵ J** against a limit of 5 × 10⁻³ J. This proves that the drive, the mouse force, the dampers and RK4 all agree with (3.14).

## Check 4: a frictionless spring pendulum

@@src live.cpp | ^    \{   // 3\) swinging spring pendulum | ^    \}$

* Example 3 with its damper **removed**: there is gravity and a spring, but nothing to burn energy. Equation (3.13) says E must stay constant forever.
* Over 30 seconds of a swinging, bouncing, partly chaotic motion the total energy wandered by **1.7 × 10⁻⁹ J** out of 23.467 J: about one part in ten billion.
* This checks the **non-linear geometry** (the angle changes, the unit vector rotates) and the gravity term, which the straight-line tests do not touch.

## Check 5: rails

@@src live.cpp | ^    \{   // 4\) rails really remove | ^    \}$

* The quarter-car has both masses on Y rails. After 3 seconds, their x coordinates must be **exactly** as they started (0). The measured sideways drift is **0.0 m**, exactly zero, not merely small. That is expected: the blocked acceleration is set to exactly zero and the blocked velocity starts at exactly zero, so every arithmetic step adds zero.

## Check 6: the frequency finder

@@src live.cpp | ^    \{   // 5\) frequency finder | ^    \}$

* Builds a **known** signal: a 2.5 Hz sine, fading slowly, 360 samples at 60 per second. The finder must return 2.5 Hz to within 0.06 Hz. It returned **2.500 Hz**.
* Testing a measuring tool on a signal whose answer you know is the oldest trick in engineering.

## Check 7: save and load

@@src live.cpp | ^    \{   // 6\) save / load round trip | ^    \}$

* Saves the quarter-car to a temporary file, loads it into a new model, and checks that the node count, link count, one stiffness and the gravity survived. Then deletes the temporary file. A **round trip** test catches mistakes in either direction, such as a field written but never read.

## Check 8: the video encoder

@@src live.cpp | ^    \{   // 7\) the video encoder | ^    \}$

* Opens a 320 × 240 file at 30 fps, writes 30 grey frames, closes it, and requires the file to exist and exceed 1000 bytes. We measured **250 857 bytes** (a solid grey frame compresses well, but the file also contains headers).
* The temporary file is deleted afterwards.

## Check 9: designing by clicking, then running

@@src live.cpp | ^    \{   // 8\) design a system from scratch | ^    \}$

This is the most unusual test: it **simulates the mouse**. It never opens a window. Instead it calls the same functions a real click would call:

* <code>click(wx, wy)</code> is a tiny lambda that converts a world position to screen pixels and calls <code>onMouseDown</code> followed by <code>onMouseUp</code>.
* The test picks the Ground tool and clicks at (−2, 0); picks the Mass tool and clicks at (1, 0); picks the Spring tool and clicks the anchor then the mass; the Damper tool the same way; then the Select tool and clicks the mass.
* It checks that the model now has **2 nodes and 2 links** and that a node is selected, sets a few properties, presses Run, and advances **600 frames of 1/60 s** (10 seconds) through the same <code>advanceSim</code> the real program uses. It then checks that the mode is still RUN, the time is above 9.9 s, more than 500 telemetry rows exist, and the energy-law error is below 10⁻⁴ J (**4.3 × 10⁻⁷ J** measured).
* Finally, with the Spring tool, it clicks on empty space at (3.3, 1.1): nothing should happen and nothing should crash.

::: why
**Why this test matters.** The earlier eight check the physics and the recorder. This one checks that **a user can actually build a system with the interface and that the system then runs**. That is the main promise of the program. It catches broken click handling, tool logic and mode switching, which physics tests can never see.
:::

## What each test would catch

| A mistake like this... | ...is caught by |
|------------------------|-----------------|
| A wrong sign in a spring force | checks 1, 2, 3, 4 |
| A damper that does not remove energy | check 2 (energy law), check 1 (formula) |
| An error in the gravity term | check 4 |
| A rail that leaks | check 5 |
| A wrong time-step or RK4 slip | checks 1, 2, 4 |
| A file written but not read back | check 7 |
| A video flipped or the encoder refused | check 8 and the orientation test |
| A click handler that does nothing | check 9 |
| A frequency finder that always answers "0 Hz" | check 6 |

## The test output

This is the real output of the final build:

```text
Vibration Lab self-test
  [PASS] 1-DOF matches theory  worst gap 6.91e-12 m after 10 s
  [PASS] 1-DOF energy law  energy-law error 3.70e-09 J
  [PASS] 2-DOF driven energy law  worst error 1.01e-05 J over 20 s (E up to 1.6 J)
  [PASS] spring pendulum conserves energy  energy wandered by 1.70e-09 J (E0 = 23.467 J)
  [PASS] Y-rail masses never move sideways  sideways drift 0.0e+00 m
  [PASS] frequency finder  found 2.500 Hz (true 2.5)
  [PASS] design file round trip
  [PASS] H.264 video encoder  30 frames written (250857 bytes)
  [PASS] design by clicking, then run  nodes 2, links 2, t = 10.00 s, energy error 4.3e-07 J

all checks passed
```

## What the tests cannot tell you

Be honest about the limits. The tests prove the **physics is right**, the **recorder produces a valid video**, and the **click logic works**. They do **not** prove that the interface looks good or is comfortable, that every possible click sequence is safe, or that it works on every Windows version and every graphics driver. Those need a human using the program. The tests give a solid floor, not a ceiling.

## Chapter checks

::: quiz
1. What exit code does <code>--selftest</code> return on failure, and why does it matter?
2. Why is the frictionless spring pendulum a good test?
3. Why is the sideways drift in check 5 exactly zero and not merely tiny?
4. How does check 9 test the interface without a window?
5. Name one thing the self-test cannot tell you.
:::

::: answers
1. A non-zero code (1). Scripts and build systems can then detect failure automatically.
2. Energy must be exactly conserved, and the geometry is fully non-linear, which tests the force directions and the gravity term.
3. The blocked acceleration is set to exactly zero and the blocked velocity starts at exactly zero, so no step ever adds anything to it.
4. It calls the same click handlers (<code>onMouseDown</code>, <code>doAction</code>) the real mouse would, then runs the simulation through the same <code>advanceSim</code>.
5. Whether it looks good or is comfortable to use, or that it works on every Windows version and graphics driver.
:::
