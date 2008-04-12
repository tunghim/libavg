//
//  libavg - Media Playback Engine. 
//  Copyright (C) 2003-2006 Ulrich von Zadow
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Current versions can be found at www.libavg.de
//

#include "FFMpegDecoder.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4251)
#endif
#include <Magick++.h>
#ifdef _WIN32
#pragma warning(pop)
#endif

#include "AsyncVideoDecoder.h"

#include "../graphics/Filterfliprgba.h"
#include "../graphics/Filterfliprgb.h"

#include "../base/TimeSource.h"
#include "../base/TestSuite.h"
#include "../base/Exception.h"
#include "../base/Profiler.h"
#include "../base/Directory.h"
#include "../base/DirEntry.h"

#include <stdio.h>
#include <string>
#include <sstream>

using namespace avg;
using namespace std;

// TODO: 
//  - Seek forward & back (longer movies)
//  - Test getNumFrames
//  - Repeat for other File formats.
//  - Remove testfiles from python tests.
//  - Test YCbCr420p, YCbCr422


class DecoderTest: public Test {
    public:
        DecoderTest(string sClassName, bool bThreadedDecoder, bool bThreadedDemuxer)
          : Test(sClassName+getDecoderName(bThreadedDecoder, bThreadedDemuxer), 2),
            m_bThreadedDecoder(bThreadedDecoder),
            m_bThreadedDemuxer(bThreadedDemuxer)
        {}

    protected:
        bool isDemuxerThreaded() 
        {
            return m_bThreadedDemuxer;
        }

        VideoDecoderPtr createDecoder() 
        {
            VideoDecoderPtr pDecoder;
            pDecoder = VideoDecoderPtr(new FFMpegDecoder());
            if (m_bThreadedDecoder) {
                pDecoder = VideoDecoderPtr(new AsyncVideoDecoder(pDecoder));
            }
            return pDecoder;
        }

    private:
        string getDecoderName(bool bThreadedDecoder, bool bThreadedDemuxer) {
            string sName = "(";
            if (bThreadedDecoder) {
                sName += "Threaded decoder, ";
            } else {
                sName += "Sync decoder, ";
            }
            if (bThreadedDemuxer) {
                return sName+string("Threaded demuxer)");
            } else {
                return sName+string("Sync demuxer)");
            }
        }

        bool m_bThreadedDecoder;
        bool m_bThreadedDemuxer;
};

class VideoDecoderTest: public DecoderTest {
    public:
        VideoDecoderTest(bool bThreadedDecoder, bool bThreadedDemuxer)
            :DecoderTest("VideoDecoderTest", bThreadedDecoder, bThreadedDemuxer)
        {}

        void runTests()
        {
            basicFileTest("mpeg1-48x48.mpg", 30);
            basicFileTest("mjpeg-48x48.avi", 202);
            seekTest("mjpeg-48x48.avi");
        }

    private:
        void basicFileTest(const string& sFilename, int ExpectedNumFrames) 
        {
            try {
                cerr << "    Testing " << sFilename << endl;

                VideoDecoderPtr pDecoder = createDecoder();
                pDecoder->open(getSrcDir()+"testfiles/"+sFilename, OGL_NONE, 
                        isDemuxerThreaded());
                IntPoint FrameSize = pDecoder->getSize();
                TEST(FrameSize == IntPoint(48, 48));
                TEST(pDecoder->getPixelFormat() == B8G8R8X8);
                BitmapPtr pBmp(new Bitmap(FrameSize, B8G8R8X8));

                // Test first two frames.
                pDecoder->renderToBmp(pBmp, -1);
                compareImages(pBmp, sFilename+"_1");
                pDecoder->renderToBmp(pBmp, -1);
                compareImages(pBmp, sFilename+"_2");
                pDecoder->close();
                
                readWholeFile(sFilename, 1, ExpectedNumFrames); 
                readWholeFile(sFilename, 0.5, ExpectedNumFrames); 
                readWholeFile(sFilename, 2, ExpectedNumFrames/2); 
            } catch (Magick::Exception & ex) {
                cerr << string(m_IndentLevel+6, ' ') << ex.what() << endl;
                throw;
            }
        }

        void seekTest(const string& sFilename)
        {
            cerr << "    Testing " << sFilename << " (seek)" << endl;

            VideoDecoderPtr pDecoder = createDecoder();
            pDecoder->open(getSrcDir()+"testfiles/"+sFilename, OGL_NONE, 
                    isDemuxerThreaded());

            IntPoint FrameSize = pDecoder->getSize();
            BitmapPtr pBmp(new Bitmap(FrameSize, B8G8R8X8));

            // Seek forward
            pDecoder->seek((long long)(100*1000/pDecoder->getNominalFPS()));
            pDecoder->renderToBmp(pBmp, -1);
            compareImages(pBmp, sFilename+"_100");

            // Seek backward
            pDecoder->seek((long long)(53*1000/pDecoder->getNominalFPS()));
            pDecoder->renderToBmp(pBmp, -1);
            compareImages(pBmp, sFilename+"_53");

            // Seek to last frame
            pDecoder->seek((long long)(201*1000/pDecoder->getNominalFPS()));
            pDecoder->renderToBmp(pBmp, -1);
            compareImages(pBmp, sFilename+"_201");

            pDecoder->close();
        }

        void readWholeFile(const string& sFilename, 
                double SpeedFactor, int ExpectedNumFrames)
        {
            // Read whole file, test last image.
            VideoDecoderPtr pDecoder = createDecoder();
            pDecoder->open(getSrcDir()+"testfiles/"+sFilename, OGL_NONE, 
                    isDemuxerThreaded());
            IntPoint FrameSize = pDecoder->getSize();
            BitmapPtr pBmp(new Bitmap(FrameSize, B8G8R8X8));
            double TimePerFrame = (1000.0/pDecoder->getFPS())*SpeedFactor;
            int NumFrames = 0;
            double CurTime = 0;

            while(!pDecoder->isEOF()) {
                FrameAvailableCode FrameAvailable = 
                        pDecoder->renderToBmp(pBmp, (long long)CurTime);
                if (FrameAvailable == FA_NEW_FRAME) {
//                    stringstream ss;
//                    ss << "testfiles/result/" << sFilename << NumFrames << ".png";
//                    pBmp->save(ss.str());
                    NumFrames++;

                } else {
                    TimeSource::get()->msleep(0);
                }
                if (FrameAvailable == FA_NEW_FRAME || FrameAvailable == FA_USE_LAST_FRAME) { 
                    CurTime += TimePerFrame;
                }
            }
//            cerr << "NumFrames: " << NumFrames << ", ExpectedNumFrames: " << ExpectedNumFrames << endl;
            TEST(NumFrames == ExpectedNumFrames);
            if (SpeedFactor == 1) {
                compareImages(pBmp, sFilename+"_end");
            }
            
            // Test loop.
            pDecoder->seek(0);
            pDecoder->renderToBmp(pBmp, -1);
            compareImages(pBmp, sFilename+"_loop");

            pDecoder->close();
        }

        void compareImages(BitmapPtr pBmp, const string& sFilename)
        {
            BitmapPtr pBaselineBmp;
            try {
                pBaselineBmp = BitmapPtr(new Bitmap(
                        getSrcDir()+"testfiles/baseline/"+sFilename+".png"));
            } catch (Magick::Exception & ex) {
                TEST_FAILED("Error loading baseline image: " << ex.what()); 
                pBmp->save(getSrcDir()+"testfiles/result/"+sFilename+".png");
                return;
            }
            FilterFlipRGB().applyInPlace(pBaselineBmp);
#ifdef __BIG_ENDIAN__
            FilterFlipRGBA().applyInPlace(pBmp);
#endif
            int DiffPixels = pBaselineBmp->getNumDifferentPixels(*pBmp);
            if (DiffPixels > 0) {
                TEST_FAILED("Error: Decoded image differs from baseline '" << 
                        sFilename << "'. " << DiffPixels << " different pixels.");
                try {
                    pBmp->save(getSrcDir()+"testfiles/result/"+sFilename+".png");
                    BitmapPtr pOrigBmp(new Bitmap(getSrcDir()+"testfiles/baseline/"+sFilename+".png"));
                    pOrigBmp->save(getSrcDir()+"testfiles/result/"+sFilename+"_baseline.png");
                    Bitmap DiffBmp(*pBmp);
                    DiffBmp.subtract(&*pBaselineBmp);
                    DiffBmp.save(getSrcDir()+"testfiles/result/"+sFilename+"_diff.png");
                } catch (Magick::Exception & ex) {
                    TEST_FAILED("Error saving result image: " << ex.what()); 
                    return;
                }
            }
        }

};

class AudioDecoderTest: public DecoderTest {
    public:
        AudioDecoderTest(bool bThreadedDecoder, bool bThreadedDemuxer)
          : DecoderTest("AudioDecoderTest", bThreadedDecoder, bThreadedDemuxer)
        {}

        void runTests()
        {
            audioTest("22.050Hz_16bit_mono.wav");

            audioTest("44.1kHz_16bit_mono.wav");
            audioTest("44.1kHz_16bit_stereo.wav");
            audioTest("44.1kHz_24bit_mono.wav");
            audioTest("44.1kHz_24bit_stereo.wav");

            audioTest("48kHz_16bit_mono.wav");
            audioTest("48kHz_16bit_stereo.wav");
            audioTest("48kHz_24bit_mono.wav");
            audioTest("48kHz_24bit_stereo.wav");

            audioTest("44.1kHz_16bit_stereo.aif");
            audioTest("44.1kHz_mono.ogg");
            audioTest("44.1kHz_stereo.ogg");
            audioTest("44.1kHz_stereo.mp3");
        }

    private:
        void audioTest(const string& sFilename)
        {
            // TODO:
            // - getCurTime()
            // - get/setSpeedFactor()
            // - get/setVolume()
            // - seek()
            try {
                cerr << "    Testing " << sFilename << endl;
                
                {
                    cerr << "      Reading complete file." << endl;
                    VideoDecoderPtr pDecoder = createDecoder();
                    pDecoder->setAudioFormat(2, 44100);
                    pDecoder->open(getSrcDir()+"testfiles/"+sFilename, OGL_NONE, 
                            isDemuxerThreaded());
                    int TotalBytesDecoded = 0;
                    bool bCheckTimestamps = (sFilename.find(".ogg") == string::npos &&
                            sFilename.find(".mp3") == string::npos);
                    readAudioToEOF(pDecoder, TotalBytesDecoded, bCheckTimestamps);

                    if (sFilename.find(".ogg") == string::npos) {
                        // Check if we've decoded the whole file.
                        // TODO: Find out what is broken with ogg files here.
                        int FramesDecoded = TotalBytesDecoded/4;
                        int FramesInDuration = pDecoder->getDuration()*44100/1000;
//                        cerr << "FramesDecoded: " << FramesDecoded << endl;
//                        cerr << "FramesInDuration: " << FramesInDuration << endl;
                        TEST (abs(FramesDecoded-FramesInDuration) < 45);
                    }
                }
                {
                    cerr << "      Seek test." << endl;
                    VideoDecoderPtr pDecoder = createDecoder();
                    pDecoder->setAudioFormat(2, 44100);
                    pDecoder->open(getSrcDir()+"testfiles/"+sFilename, OGL_NONE, 
                            isDemuxerThreaded());
                    long long Duration = pDecoder->getDuration();
                    pDecoder->seek(Duration/2);
                    unsigned char AudioBuffer[16];
                    pDecoder->fillAudioFrame(AudioBuffer, 16);
                    TEST(abs(Duration/2-pDecoder->getCurTime()) < 60); // 60 ms accuracy for seeks.
                    int TotalBytesDecoded = 16;

                    readAudioToEOF(pDecoder, TotalBytesDecoded, false);
                    if (sFilename.find(".ogg") == string::npos) {
                        // Check if we've decoded half the file.
                        int FramesDecoded = TotalBytesDecoded/4;
                        int FramesInDuration = pDecoder->getDuration()*44100/1000;
//                        cerr << "FramesDecoded: " << FramesDecoded << endl;
//                        cerr << "FramesInDuration: " << FramesInDuration << endl;
                        TEST (abs(FramesDecoded-FramesInDuration/2) < 45);
                    }

                }

            } catch (Magick::Exception & ex) {
                cerr << string(m_IndentLevel+6, ' ') << ex.what() << endl;
                throw;
            }
        }

        void readAudioToEOF(VideoDecoderPtr pDecoder, int& TotalBytesDecoded, 
                bool bCheckTimestamps) 
        {
            int NumWrongTimestamps = 0;
            while(!pDecoder->isEOF()) {
                unsigned char AudioBuffer[1024];
                int BytesDecoded = 0;
                while (BytesDecoded == 0 && !pDecoder->isEOF()) {
                    BytesDecoded = pDecoder->fillAudioFrame(AudioBuffer, 1024);
                    TimeSource::get()->msleep(0);
                }
                TotalBytesDecoded += BytesDecoded;
                long long CurTime = (TotalBytesDecoded/4)/44.1;
                if (abs(CurTime-pDecoder->getCurTime()) > 20) {
                    NumWrongTimestamps++;
                }
//                cerr << CurTime << "->" << pDecoder->getCurTime() << endl;
            }
            if (bCheckTimestamps) {
                if (NumWrongTimestamps>0) {
                    TEST_FAILED(NumWrongTimestamps << " wrong timestamps.");
                }
            }
        }
        
};


class VideoTestSuite: public TestSuite {
public:
    VideoTestSuite() 
        : TestSuite("VideoTestSuite")
    {
        addTest(TestPtr(new VideoDecoderTest(false, false)));
        addTest(TestPtr(new VideoDecoderTest(false, true)));
        addTest(TestPtr(new VideoDecoderTest(true, false)));
        addTest(TestPtr(new VideoDecoderTest(true, true)));
        
        addTest(TestPtr(new AudioDecoderTest(false, false)));
        addTest(TestPtr(new AudioDecoderTest(false, true)));
        addTest(TestPtr(new AudioDecoderTest(true, false)));
        addTest(TestPtr(new AudioDecoderTest(true, true)));
    }
};


void deleteOldResultImages() 
{
    string sDirName("testfiles/result/");
    Directory Dir(sDirName);
    int err = Dir.open(true);
    if (err) {
        cerr << "Creating directory " << sDirName << " failed." << strerror(err) << endl;
    } else {
        cerr << "Deleting files in " << sDirName << endl;
        DirEntryPtr pEntry = Dir.getNextEntry();
        while (pEntry) {
            if (pEntry->getName()[0] != '.') {
                pEntry->remove();
            }
            pEntry = Dir.getNextEntry();
        }
    }
}

int main(int nargs, char** args)
{
    ThreadProfilerPtr pThreadProfiler = ThreadProfilerPtr(new ThreadProfiler("Main"));
    Profiler::get().registerThreadProfiler(pThreadProfiler);

    deleteOldResultImages();

    VideoTestSuite Suite;

    Suite.runTests();
    bool bOK = Suite.isOk();

    if (bOK) {
        return 0;
    } else {
        return 1;
    }
}

