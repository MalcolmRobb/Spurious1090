Spurious1090 README
===

Spurious 1090 is a Mode A/C/S display utility designed to work with any
ModeA/C/S receiver that provides TCP/IP output in a Beast style data 
stream.

It can be used in the same way as View1090, but displays the data 
received in a statistical way. The idea is to try and get receiver 
manufacturers to improve the quality of the data they provide, and
not just concentrate on quantity.

Installation
---

There is no installation. If you don't trust executables written by idiots 
like me and downloaded from t'interweb then the source code is provided in 
the root directory together with a Visual Studio 6.0 C++ project file. Download
it, inspect the source files and then compile it yourself.

There is no Linux make file, although since the code is derived from dump1090 
it shouldn't be too difficult for someone to build it under Linux if you so wish.

The pre-compiled Windows exe is available in both Release and Debug forms in the 
associated sub directories. I've also included all the non standard windows 
DLL's, and a sample batch file to kick things off.

I suggest most users just download all the files in the /Release subdirectory and
put them in a suitable directory on your machine - probably called spurious1090.

Open the batch file in a text editor of your choice, and change the 
--net-bo-ipaddr and --net-bo-port parameters to point at the TCP/IP address
and port of your receiver. Then execute the batch file and you should see a
screen full of numbers appear.

The program has been tested on Win7Pro-32bit and Win10Pro-64. It's a simple
console app, so should work on virtually any 32 or 64 bit version of Windows 
from NT4 up.

Normal Output
---

So what do all the numbers mean? Some of them will be self evident, some 
perhaps not so.

ModeS Frames (All)
---
This is the total count of all the Mode S frames output by your receiver.

To the right of this the individual Download Formats (DF) counts are 
displayed. There are 32 DF types defined in the ICAO specificaiton for
Mode S, although only some of them are actually used.

Specifically DF0, DF4, DF5, DF11, DF16, DF17, DF18, DF20, DF21 are
relatively common, and you should see these counts ticking up.

The other DF's are either undefined, or for military use, so in theory you
probably shouldn't see any of these. You may see the occasional spurious
frame from your receiver though.

ModeS Altitude Frames
---
This is the number of Mode S frames containing altitude information.

Mode S Altitude information is included in DF types DF0, DF4, DF16 and DF20.
The individual counts for these DF types is displayed to the right.

ModeS Squawk Frames
---
This is the number of Mode S frames containing squawk information.

Mode S Altitude information is included in DF types DF5, DF21.
The individual counts for these DF types is displayed to the right.

ModeS Short Duplicates
---
This is an annoying feature of several receiver types. They output
the same Frame several times, with a timestamp only a few micro-seconds
apart. 

A Short ModeS Frame consists of an 8uS preamble followed by a 56 bit
data stream. The total time to transmit this whole frame is therefore 
64uS. 

If we see two Frames containing identical data, and whose timestamps 
differ by less than 64uS then the two frames cannot possibly both be 
'real'. Your receiver must have output the same frame twice - Why?

Receiver manufacturers don't seem to care about this because they aren't 
feeding false data, and it does increase their apparant "Message rates"
which is all many of them care about. Of course you can double your 
message rate if you output the same frame twice - but you aren't adding 
any extra information by doing it.

ModeS Short Overlapping
---
A Short ModeS Frame consists of an 8uS preamble followed by a 56 bit
data stream. The total time to transmit this whole frame is therefore 
64uS. 

If we see two Frames containing different data, and whose timestamps 
differ by less than 64uS then the two frames cannot possibly both be 
'real' since they must overlap in time.

This is curious because most ModeS message types are CRC protected, 
so if either of the two messages where corrupt the CRC should have 
rejected them. So how can they both pass CRC checking, but overlap 
in time - huhh?

ModeS Long Duplicates
---
This is an annoying feature of several receiver types. They output
the same Frame several times, with a timestamp only a few micro-seconds
apart. 

A Long ModeS Frame consists of an 8uS preamble followed by a 112 bit
data stream. The total time to transmit this whole frame is therefore 
120uS. 

If we see two Frames containing identical data, and whose timestamps 
differ by less than 120uS then the two frames cannot possibly both be 
'real'. Your receiver must have output the same frame twice - Why?

Receiver manufacturers don't seem to care about this because they aren't 
feeding false data, and it does increase their apparant "Message rates"
which is all many of them care about. Of course you can double your 
message rate if you output the same frame twice - but you aren't adding 
any extra information by doing it.

ModeS Long Overlapping
---
A Long ModeS Frame consists of an 8uS preamble followed by a 112 bit
data stream. The total time to transmit this whole frame is therefore 
120uS. 

If we see two Frames containing different data, and whose timestamps 
differ by less than 120uS then the two frames cannot possibly both be 
'real' since they must overlap in time.

This is curious because most ModeS message types are CRC protected, 
so if either of the two messages where corrupt the CRC should have 
rejected them. So how can they both pass CRC checking, but overlap 
in time - huhh?

ModeAC Frames (All)
---
This is the total number of ModeA/C frames output by the receiver.

Comnpare the count of ModeAC frames to the count of ModeS frames.
In the UK, good receivers (Beast) typically show about 2.5 times the 
number of ModeA/C Frames as Mode S.

Lesser receivers (Dongles) typically have ModeAC:ModeS ratios worse
than 1:1 - often much worse - like 1:4 rather than 2.5:1 often seen 
with a Beast. The implication then is that the dongle is missing 
somewhere in excess of 90% of all ModeAC frames.

The rates will vary due to location around the world, and depend on 
what your local radar sites are actually asking for - if they are 
only asking for ModeS then of course you won't see any ModeAC.

ModeA Frames (Matching ModeS Squawk)
---
This is the count of the number of ModeA frames which exactly match a 
known ModeS_A code (ie a Mode A code derived from ModeS DF5/DF21 data).

Add the "ModeA Frames (Matching ModeS Squawk)" count to the 
"ModeC Frames (Matching ModeS Altitude)" and then divide by 
"ModeAC Frames (All)".

That gives you a percentage of valid (i.e. non-spurious) ModeAC codes
received. See the problem? Few receivers give better than 70% valid codes.

ModeC Frames (Matching ModeS Altitude)
---
This is the count of the number of ModeC frames which exactly match a 
known ModeS_C code (ie a Mode C code derived from ModeS 
DF0/DF4/DF16/DF20 data).

Add the "ModeA Frames (Matching ModeS Squawk)" count to the 
"ModeC Frames (Matching ModeS Altitude)" and then divide by 
"ModeAC Frames (All)".

That gives you a percentage of valid (i.e. non-spurious) ModeAC codes
received. See the problem? Few receivers give better than 70% valid codes.

ModeAC Frames (Duplicates)
---
A ModeAC Frame consists of 15 bit times, each bit period lasting 1.45uS. 
Therefore if takes 21.75uS to transmit the frame. Things are slightly
complicated by the SPI (Squawk Ident) bit which occurs outside the main 
frame at bit time 24.65uS, lasting another 1.45uS, so 26.1uS

We can argue about what the limit should be here, but I've decided it 
should be 26.1uS - which covers the SPI bit.

So if we get the same ModeAC code twice within a 26.1uS interval then either
there are two aircraft transmitting the same code synchronously (unlikely)
or the receiver has output the same frame twice (naughty naughty).

ModeAC Frames (Overlapping)
---
A ModeAC Frame consists of 15 bit times, each bit period lasting 1.45uS. 
Therefore it takes 21.75uS to transmit the frame. Things are slightly
complicated by the SPI (Squawk Ident) bit which occurs outside the main 
frame at bit time 24.65uS, lasting another 1.45uS, so 26.1uS

We can argue about what the limit should be here, but I'lll argue it 
should be 26.1uS - which covers the SPI bit - untill someone convinces me 
otherwise.

The '1' bits in a ModeAC only last 0.45uS, and then there is a 1uS space 
before the next bit. It is therefore possible for ModeAC codes from two 
closely spaced aircraft to be interleaved in such a way that both can be 
successfully received. see https://www.radartutorial.eu/13.ssr/sr15.en.html

However, for non-synchronous de-garbling to work the bits of the two frames 
must be separated from each other by some odd multiple of half a bit period, 
plus or minus a bit.

The Overlapping count is the number of ModeAC frames which occur less than 26.1uS
after the previous frame, and the data is different to the previous frame, and the 
timestamps indicate the '1' bits must overlap. This means that both this frame, and 
the previous one should be considered garbled, and hence unreliable.

For instance, a typical decode might be (these are real - received by me yesterday!)

5424 received at timestamp +0
2612 received at timestamp +35 (+2.92uS, or 2.01 bit times later)
6616 received at timestamp +36 (+3.00uS, or 2.07 bit times later)
6627 received at timestamp +54 (+4.5uS, or 3.10 bit times later)

If we draw out what the off-air bit pattern would have been for these 4 codes, and delay 
each code by the timestamp offset in bit periods we get..

5424 = 101100100000111
2612 =   110010000011101
6616 =   110010000011111
6627 =    100100000111111

See what is going on? 4 codes reported, different timestamps, and no way of knowing which, 
if any, is valid.

ModeAC Frames (Interleaved)
---
Same description as above in "ModeAC Frames (Overlapping)", except the timestamps 
indicate the '1' bits are time aligned such that they interleave correctly and do 
not overlap. 

This means the two frames may be correctly decoded and hence valid. I wouldn't bet 
too much money on it though.

ModeAC Frames (XX0000)
---
Some receivers (Beast for example) spew out an almost constant stream of
useless XX0000 codes day and night. These are almost always spurious, and 
probably generated by noise. In my location about 10% of all ModeAC frames 
output are XX0000.

ModeAC Frames (One Bit Set)
---
This count is the number of ModeAC frames that had only one data bit set. 
There are 12 such codes : XX0001, XX0002, XX0004, XX0010, XX0020, XX0040, 
XX0100, XX0200, XX0400, XX1000, XX2000 and XX4000. Some of these might be 
valid, but chances are that most are spurious.

ModeA Frames (One Bit Zero Errors)
---
This is the count of the number of ModeA frames which do not exactly match
a known ModeS_A code (i.e. a ModeA code derived from ModeS data), but do 
match a known ModeS_A code if you flip one (and only one) of the '0' bits 
to a '1' in the received ModeA. 

So, for instance, if we know (From ModeS DF5/DF21) that an aircraft should 
be Squawking 6401, and we receive a ModeA 6400, and there is no known 
aircraft using 6400, then by flipping bit 0 from a '0' to a '1' we can get 
6401 and achieve a 'hit'. 

This means that the receiver has possibly mis-interpreted the radio signal
which 'should' have been 6401, and spuriously decoded '6400' instead.

ModeA Frames (One Bit One Errors)
---
This is the count of the number of ModeA frames which do not exactly match
a known ModeS_A code (ie a Mode A code derived from ModeS data), but do 
match a known ModeS_A code if you flip one (and only one) of the '1' bits 
to a '0' in the received ModeA. 

So, for instance, if we know (From ModeS DF5/DF21) that an aircraft should be
Squawking 6401, and we receive a ModeA 6403, and there is no known aircraft
using 6403, then by flipping bit 1 from a '1' to a '0' we can get 6401 and
achieve a 'hit'. 

This means that the receiver has possibly mis-interpreted the radio signal
which 'should' have been 6401, and spuriously decoded '6403' instead.

ModeC Frames (One Bit Zero Errors)
---
This is the count of the number of ModeC frames which do not exactly match
a known ModeS_C code (ie a ModeC code derived from ModeS data), but do 
match a known ModeS_C code if you flip one (and only one) of the '0' bits 
to a '1' in the received ModeC. 

So, for instance, if we know (From ModeS DF0/DF4/DF16/DF21) that an aircraft 
should be emmitting a code corrosponding to FL330 (which is 1324), and we 
receive code 0324, and there is no known aircraft flying at FL605 (which is 
what 0324 represents) then by flipping bit 9 from a '0' to a '1' we can get 
1324 and achieve a 'hit'. 

This means that the receiver has possibly mis-interpreted the radio signal
which 'should' have been 1324, and spuriously decoded '0324' instead.

ModeC Frames (One Bit One Errors)
---
This is the count of the number of ModeC frames which do not exactly match
a known ModeS_C code (ie a ModeC code derived from ModeS data), but do 
match a known ModeS_C code if you flip one (and only one) of the '1' bits 
to a '0' in the received ModeC. 

So, for instance, if we know (From ModeS DF0/DF4/DF16/DF21) that an aircraft 
should be emmitting a code corrosponding to FL330 (which is 1324), and we 
receive code 1724, and there is no known aircraft flying at FL335 (which is 
what 1724 represents) then by flipping bit 8 from a '0' to a '1' we can get 
1324 and achieve a 'hit'. 

This means that the receiver has possibly mis-interpreted the radio signal
which 'should' have been 1324, and spuriously decoded '1724' instead.

ModeAC Frames (Matching Nothing)
---
This is the number of frames that the receiver has output which do not exactly
match a Known ModeS_A or a Known ModeS_C, are'nt XX0000, aren't One bit set,
and can't be explained by One bit one or one bit zero errors.

In other words, we don't know what on earth they are. They are probably a mix
of the following...

1) Completely spurious noise induced codes.
2) Spurious codes caused by receiver decoder generated errors/mistakes.
3) Real Mode 3A/3C codes from an aircraft not equipped with a Mode S transponder
4) Real Mode 3A/3C codes from an aircraft with it's Mode S transponder turned off
5) Real Mode 1/2 SSR codes - often from Military or Govt aircraft.

If you live close to a light aircraft airfield then 3 or 4 may be likely, but 
probably only during daylight hours and in good weather.

If you live close to a busy military airfield then 3, 4 and 5 are all possible.

However, I'll stick my neck out and say that for most users 1 and 2 will be the
primary cause of spurious codes. Hence spurious1090.

Contributing
---


Credits
---
Spurious1090 was written by Malcolm Robb, and is derived from an original
codebase developed originally for Dump1090 by Salvatore Sanfilippo 
<antirez@gmail.com> which is released under the BSD three clause license.
