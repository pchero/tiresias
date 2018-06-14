tiresias
========

Audio fingerprinting and recognition algorithm implemented in c, using the MFCC(Mel-frequency cepstral coefficients).
[MFCC](https://en.wikipedia.org/wiki/Mel-frequency_cepstrum)

Tiresias can memorize audio by listening to it once and fingerprinting it. Then by playing a audio Triresias attempts to match the audio against the fingerprints held in the database, returning the audio being plyed.

## Installation
```
$ cd src
$ make
```

## Quick start
### Usage
```
$ ./tiresias
Usage: 
./tiresias -a <fingerprint filename> : Add the audio fingerprint info.
./tiresias -d <audio uuid> : Delete the audio fingerprint info.
./tiresias -s <search filename> : Search audio fingerprint info.
./tiresias -l : Show all list of fingerprinted lists.
```

### Fingerprinting
To use the audio recogntion, you need to add the target audio's fingerprint infomation first. Once you add the audio file, all the fingerprint infomation will be stored to the sqlite3 database.
```
$ ./tiresias -a ./test1.wav 
```

### Recognition
Run the tiresias with -s option, it returns most matched audio infomation info.
```
$ ./tiresias -s ./wav_test/test1.wav
Audio name:  test1.wav
Audio uuid:  334fe499-2ff3-4157-bb8b-06f90203e65d
Audio hash:  40ecddd443d0284e8a18b5be5336e7c2
Frame count: 3524
Match count: 3524

$ ./tiresias -s ./wav_test/test1_part2.wav
Audio name:  test1.wav
Audio uuid:  334fe499-2ff3-4157-bb8b-06f90203e65d
Audio hash:  40ecddd443d0284e8a18b5be5336e7c2
Frame count: 1054
Match count: 897
```

## License
* BSD License

## Remember 2014.04.16
* Still remember 2014.04.16
* Rest in peace..
* https://en.wikipedia.org/wiki/Sinking_of_MV_Sewol
