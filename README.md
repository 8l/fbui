# fbui
Framebuffer UI (fbui) in-kernel Linux windowing system.

Copyright © 2004-2010 by Zack Smith.
All rights reserved.
Introduction
FBUI, or FrameBufferUI, is a small, in-kernel windowing system for Linux that I created largely to address the issue of software bloat. I wrote it to work with Linux kernel 2.6.9 and I've subsequently gotten it partly working with 2.6.31.

A summary of its key features:

* It is very small, about 50kB.
* It lives inside the Linux kernel, which places a natural limit on GUI bloat.
* It permits multiple programs to share the framebuffer by letting each have graphical windows.
* Each program may have multiple windows.
* Windows may overlap, and be moved, resized, raised, lowered etc.
* There can be windows on each virtual console.
* Program interaction with FBUI is via a small set of system calls (ioctls).
* Drawing primitives now support transparency.
* It includes a small library libfbui to make using FBUI easier, and it includes an image-manipulation library and a font library.

Goals

* FBUI exists firstly to reduce the software bloat that plagues modern operating systems. It does this by virtue of its being a simple windowing system in the form of a small, 50 kilobyte driver, which for some purposes and users may be perfectly sufficient. Liberation from bloat is desirable for a number of reasons that I explain in the Philosophy section.
* FBUI exists to assist people who are prohibited from using X Windows because they are using resource-limited platforms such as old computers and embedded devices. On these, X is an impossible burden. However a vanilla framebuffer is often far too primitive. FBUI is "just right", and libfbui makes using FBUI even easier to use by providing abstractions and additional functions. Note, FBUI is also a delight to use on faster computers.
* FBUI exists to correct a flaw in the Linux operating system architecture. The traditional GUI -- X Windows -- is unlike any other subsystem of Linux in that the hardware-accelerated video drivers it uses are located within the X server, which is outside the kernel. Notice: normally Linux drivers and vital subsystems such as keyboard, USB, filesystem, serial I/O, et cetera are all located inside the kernel. FBUI simply puts the windowing system where it belongs: inside the kernel with all the other drivers.
* FBUI could potentially provide a means to use all of video memory, which on modern systems can be over 1024 megabytes (the eVGA Geforce 7950CX2). An OS like Linux is simply not designed to make that much video memory available to userspace with 32-bit addressing. When managed from within the kernel however, it can be accessed in pages and the entirely of it can be used to store all sorts of graphics data, including window data, fonts, et cetera.
* It's conceivable that FBUI could be the basis for a desktop Linux focused on using older computers. This is assuming that 3D graphics will not become an emphasis.

Dealing With Superstition
Some would say, "But Microsoft tried the in-kernel graphics thing, and look what a mess it is!"

It is true that (A) Microsoft's pre-XP OSes were not stable or well designed and (B) Microsoft has used an in-kernel GUI approach. But to assume that B implies A is poor reasoning, as is the assumption that any such implication is valid for other developers. Such bad reasoning or related superstition ignore the true causes of Microsoft's failures, which are:

* Microsoft project managers and/or programmers have proven themselves to be poor at more than just creating stable operating systems, which suggests poor project management or design, and/or poor coding and/or insufficient testing. They are well compensated but their software is bloated and with bloat naturally you get mistakes.
* Microsoft often just borrows its ideas from others. Anyone who merely copies others' work is bound to make mistakes due to lack of understanding and/or ability.
* Microsoft has a different goal than I do: their goals are money and power, which are corrupting factors, whereas my goal is to create an effective and efficient software subsystem having the smallest possible size. History is littered with examples of companies that operated purely by the profit motive and who cut corners or skipped necessary steps, leading to disaster.
* Microsoft's windowing system is, like X Windows, a big, bloated piece of software and as every engineer knows, the larger and more complex a system or subsystem is the more likely it is that there will be bugs.
* Windows uses only two of the four x86 protection rings to prevent system crashes. Microsoft put drivers in Ring 0 and user programs in Ring 3. This is riskier than some OSes which put drivers in Ring 1 or 2 to prevent bad driver code from crashing the kernel. But, guess what? Linux does the same thing. So, why do people generally believe that Windows crashes more than Linux? Most likely it is because Windows drivers are less reliable than Linux drivers for two reasons:
1. Hardware manufacturers don't want to spend much time and money on driver development, which leads to errors.
2. Closed-source software in general prevents people who could identify errors from doing so.
In other words, private enterprise is, in the case of Windows, less effective at producing a quality product compared to what unhindered people working in the Commons can achieve.

Philosophy
Liberation from Bloat
FBUI's most important role in my opinion is the reduction of software bloat. Why make a fuss about bloat? Software bloat is bad because:

* Software bloat keeps us all on the treadmill of always buying new hardware, which ultimately new software makes painfully slow, thus we are always falling behind. But the software makes the system slow because it is poorly designed and poorly implemented and rushed work, with the frequent consequence that it is bloated. Thus the purchasing-treadmill is economically and materially wasteful. It profits the few while making the many suffer unnecessarily.
* Bloat is also bad for the Environment (which we live in and rely upon) since the manufacture of computer equipment involves the use of numerous very nasty chemicals which inevitably end up in the soil, water and air. Similarly the disposal of electronics results in chemicals leaching out of circuit boards, LCDs (which contain mercury) et cetera, which then enter the biosphere. We cannot afford to pretend this problem doesn't exist and we cannot afford to leave it to self-serving politicians to solve. It is better to solve the problem at the source: buy less hardware. ( Article )
* Liberation from bloat is liberation from rushed work, poorly managed projects, and bad engineering. It is liberation from those project managers and programmers who, rather than produce better, leaner, less buggy software, pass on the consequences of their bad choice to users who must pay to upgrade their hardware to accommodate the bloat. And as that software gets bigger and bulk is piled upon bulk, increasing numbers of bugs and vulnerabilities arise which require, you guessed it, more upgrades.

Windowing as a kind of hardware-like multiplexing
One of the underlying concepts and motivations for FBUI is the idea that a window can be viewed like a piece of hardware : a set of physical pixels. In this sense, the OS just does what it minimally should: it multiplexes access to the (abstract) hardware items, i.e. windows. This is a key reason why FBUI belongs in the kernel, because the bare minimum that a kernel should do is multiplex access to hardware. Even exokernels, which place every driver in userspace, are considered multiplexers.
In the kernel, you say?
Yes. Besides limiting GUI bloat, it also minimizes the interface to the windowing system to being a simple and elegant ioctl interface.

FBUI could be improved so as to entirely replace the framebuffer device and framebuffer console so that you would never deal with any of that mess.

FBUI could provide the basis for a completely graphical startup à la Macintosh.
The origins of bloat
The causes of bloat are many, but I shall identify a few. Consider this as food for thought.

1. In the Linux community, people often don't want to do the real work required to achieve a serious goal. Writing software like FBUI is not easy and many people would prefer instead to patch together existing bloated solutions. For example: The inappropriate use of X and GTK+ in the OpenMoko phone software project. Or the many large software projects that are impossible to build because components are broken.
2. In universities, students are taught to always reuse code. While this seems virtuous, it leads to bloat and can decrease software reliability and performance. Bloated code easily wipes out the L2 cache, slowing down modest processors substantially. But more important is the fact that the rule of always reusing code actually encourages programmers to become lazy and to fear discomfort. They become a bit like dainty urbanites on a camping trip, unwilling to do a little rigorous work.
3. Some geeks use computers as social symbols to expand their sense of personal worth. As with men who obsess over muscle cars or even SUVs, one hears them bragging about their new 3 GHz CPUs and 42-inch displays or their luxurious Macbooks. Frugal or responsible programming practices do not enter the discussion. The landfills that contain their older, formerly useful computers are "out of sight, out of mind".

Comparison
Item FBUI 0.11.0
(in kB) X-Windows / XFree86 (in kB)
Core software ~50 1710
Library software ~30 1370 (just Xlib; required)
Panel-style window manager program 46½ (fbpm / static link) not available
Conventional window manager 41 (fbwm* / static link) 145 (twm / dynamic link)
Terminal emulator program 73 (fbterm / static link) 247 (xterm / dynamic link)
Analog clock program size 43½ (fbclock / static link) 29 (xclock / dynamic link)
Simple calculator program size 43½ (fbcalc / static link) ? (xcalc / dynamic link)
JPEG/TIFF viewer 60 (fbview / static link) 962 (xv / dynamic link)

* - Note, fbwm and fbpm are not fully functional software. They are works in progress.
Screenshots


From older versions of FBUI

Screenshot while running fbpm
Screenshot while playing mpeg

Screenshot from an older version running fbwm
Child windows
FramebufferUI does not support child windows of windows, also known as subwindows or nested windows. There is a good reason for this. Child windows lead to bloat.

1. They cause bloat by being commonly used to implement GUI widgets. This method of creating widgets was proven in the days of X/Motif to be much slower and more wasteful than using the parent window. The difference was perceptible. More.
2. They cause bloat by being implemented but unnecessary.

Features

* Since there is no large server to start, individual programs start up instantaneously, as they did for instance under MSDOS.
* Unlike X Windows, FBUI supports windows on every virtual console. Switching between consoles is fast.
* Each program may have more than one window.
* FBUI 0.11 supports overlapping windows.
* Programs can receive raw keystrokes from FBUI which they can then translate to ASCII using a library routine. One process is permitted to have keyboard focus.
* Each process accesses its windows completely independently of all other processes.
In X, the library has to send all drawing commands to the server process, which puts them in a queue and executes them whenever it has a chance. If the server is busy, or another X application is flooding the queue, then an X application must wait. Not so with FBUI, where the ioctl takes a list of drawing commands that go directly to be executed if the window is visible and irregardless of what any other window is doing. To further ensure the above concurrency is the norm, use of semaphores within FBUI to access common data is made as brief as possible.
* Each virtual console can have its own optional window manager process. But this is not necessary and for instance many programs that I've written are also designed to run as the only window on the display, examples being fbview, fbcalc, fbscribble, and the my FBUI variant of mpeg2decode.
* I'm providing a very simple window manager fbwm, plus a panel-based manager fbpm, which organizes the display space for effecient work and play.
* FBUI offers a good set of drawing routines:
o draw point, line, horizontal line, vertical line, rectangle
o window clear, fill rectangle, clear rectangle
o copy area
o triangle fill
o put pixels (2-byte RGB, 3-byte RGB, 4-byte (unsigned long) RGB, monochrome 1bpp, and 4-byte RGB + transparency)
o wait for event
o poll for event
o the window manager process can hide and unhide other processes' windows, move, resize, re-expose, and delete windows.
o read point
o etc.
* FBUI is currently written for 16,24, and 32-bit RGB displays, particularly VESA.
* Sample programs provided (I suppose I've gotten carried away) :
o panel-based window manager (current focus of work)
o conventional window manager
o JPEG+TIFF image viewer
o very simple MPEG playback based on circa 1995 MPEG2 library
o terminal emulator (based on ggiterm)
o load monitor
o "scribbler" drawing program
o analog clock
o simple calculator
o "Start" button program that invokes fblauncher menu program
o POP3 mail checker
o "to do list" displayer program

Live CD
Dusan Halicky has produced an FBUI/Linux Live CD that demonstrates very clearly how effective FBUI is. Simply use his ISO to create a bootable CD and boot it. You can run FBUI programs immediately after logging in. I've tested it on two systems (Toshiba A105 with USB mouse, and a Dell PIII/700 desktop) and found it worked very well. Find it here: vdl.halicky.sk/fbui. I suggest that after logging in you run fbterm, and then fbpm in the background followed by other programs e.g. fbcalc.
Documentation
This diagram provides a rough representation of how fbui works.
Project status
Work on FBUI has basically ceased. I've updated it for kernel 2.6.31 but at present I am mainly just using its code for other projects.

After working on FBUI, I began developing a user-space equivalent called FWE (Frugal Windowing Environment), but I have ceased work on that as well. There already exist several userland windowing systems that are more advanced.

Since working on FBUI and FWE, my focus shifted to creating a small widget set, first for X Windows, which I have since scrapped, and now for Win32. My FrugalWidgets project.
Download fbui for kernel 2.6.9

* FBUI 0.11.2
o FBUI kernel code (note, 32bpp is not compiling due to a typo; use 16 or 24)
o userspace code (libfbui & programs)
* Older FBUI no overlapping windows support: FBUI 0.9.14c

Code S and tatus: alpha
Send bug reports via the email address below.

cd /usr/src/linux
cp .. fbui*.tar.bz2
tar jxfv fbui*.tar.bz2

Download fbui for kernel 2.6.31
This is my unfinished adaptation of FBUI for 2.6.31, in the form of the entire kernel tree. At present, the keyboard input code is not working but the mouse is. If you want to try fixing that, look at fbui-input.c in /usr/src/linux-2.6.31/drivers/input.

Here is the patch for linux 2.6.31.

Or you can download the modified kernel source tree: linux-2.6.31-fbui2.tar.gz

Try it out the kernel itself on a Pentium Pro or better: bzImage-fbui2
Installation for Non-Programmers

1. FBUI resides inside the 2.6.9 kernel, so the first thing you must do is to get the kernel, un-tar FBUI in its directory, select the necessary options mentioned in the README, then make the kernel and update your loader to let you boot the new kernel. (I will offer a precompiled x86 kernel later.)
2. You also need to tell your boot loader to switch to the VESA console during booting. In LILO use the expression vga=792 for a 1024x768 display or vga=789 for 800x600.
3. Then you boot with the kernel. Next you need to set up the PCF font directory, populating it with fonts from the X distribution, making sure to uncompress them. The PCF font reader is really just a temporary chunk of code so I'm not going to update it to perform automatic decompression. Note, if you aren't sure where the fonts are, type (as root) find / -name "*.pcf*". To make sure libfbui knows where they are, you can use the PCFFONTDIR environment variable (as in export PCFFONTDIR=/path...).
4. Once you've done these things, just compile the sample programs in /usr/src/linux-2.6.9/libfbui and run them from there. You may find it helps to run a program in a different virtual console using the -c switch.

Conclusions
FBUI addresses one aspect of software bloat: The windowing system.

FBUI does not address the problem of other, hugely bloated pieces of software such as the web browser, the plethora of libraries or the widget system (GTK+, Qt). (My frugal widget system is FrugalWidgets.

To really reform computer software systems to address bloat two things will be required:

1. An overhaul of the most bloated software systems.
2. An effort to address the programming culture of reckless wastefulness that has led to so much bloatware.

No doubt this reckless wastefulness is tied to the unthinking adoption of new technology, which so often disappoints because it is neither new nor clever.

I continue to offer FBUI because it demonstrates that something can be done to address bloat, which is a significant failing of the computer industry. 
