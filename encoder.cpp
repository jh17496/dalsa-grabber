/*
    A simple wrapper around the ffmpeg piper with a SPSC work queue
    Thanks: http://www.boost.org/doc/libs/1_53_0/doc/html/lockfree/examples.html
*/

#ifndef __ENCODER_CPP__
#define __ENCODER_CPP__

#include <boost/thread/thread.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>

#include <opencv2/opencv.hpp>
using namespace cv;

#include <ctime>
using namespace std;
#include <chrono>
using namespace std::chrono;
 

#include "ReadWriteMoviesWithOpenCV/DataManagement/VideoIO.h"


const int queueCapacity = 64;

// As a proportion of the image size (TODO: option?)
const float displayScale = 0.25;

milliseconds time_ms()
{
    return duration_cast< milliseconds >(
        system_clock::now().time_since_epoch()
    );
}



class VideoEncoder 
{
    private:
        int count;
        VideoIO _writer;
        boost::atomic<bool> _done;
        boost::thread* _encoderThread;
        boost::lockfree::spsc_queue<cv::Mat, boost::lockfree::capacity<queueCapacity> > _queue;

        void ffmpegWorker(void)
        {
            // int value;
            while (!_done) 
            {
                writeQueue();
            }
            writeQueue();
        }

        void writeQueue()
        {
            cv::Mat img;
            while (_queue.pop(img))
            {
                // TODO: handle status
                count++;
                milliseconds renderStart = time_ms();

                // Pass to ffmpeg
                int status = _writer.WriteFrame(img);

                // Pass to display
                auto cloneStart = time_ms();
                if(!_displaying)
                {
                    cv::resize(img, displayImg, cv::Size(), displayScale, displayScale);
                    boost::thread* displayThread = new boost::thread(boost::bind(&VideoEncoder::displayFrame, this));
                }

                milliseconds renderEnd = time_ms();

                auto renderTime = (renderEnd - renderStart);
                cout << "writeQueue() # " << count << endl;
                cout << "\twriteQueue time: " << std::to_string(renderTime.count()) << endl;
                cout << "\tencoder stack available: " << _queue.read_available() << endl;

                img.release();
            }
        }

        // TODO: Quit on 'q'
        cv::Mat displayImg;
        // TODO: Use a lock
        bool _displaying = false;
        void displayFrame(void)
        {
            _displaying = true;

            // TODO: This try-catch is careless and sloppy, but it throws an exception on the last frame
            try
            {
                imshow(windowName, displayImg);
                int key = waitKey(1);
                if((char) key == 'q') 
                {
                    cout << "Quitting display...\n";
                }

                displayImg.release();
            }
            catch(...){}

            _displaying = false;
        }

    public:
        int writeFrameCount = 0;
        const char windowName[14] = "Dalsa Monitor";

        VideoEncoder(char*, int, int, int);

        //TODO feedback if something has failed
        int writeFrame(cv::Mat img)
        {
            writeFrameCount++;

            milliseconds renderStart = time_ms();
            _queue.push(img);
            milliseconds renderEnd = time_ms();
        }

        // Hangs until all frames have been passed to ffmpeg
        int close()
        {
            _done = true;
            _encoderThread->join();
        }

};

VideoEncoder::VideoEncoder(char filename[], int width, int height, int framerate)
{
    count = 0;

    char ffmpegOptions[] = "-y -crf 17 -codec:v libx264 -preset ultrafast";
    _writer.DebugMode = true;
    _writer.Create(filename, width, height, framerate, ffmpegOptions);

    // Start thread
    _done = false;
    _encoderThread = new boost::thread(boost::bind(&VideoEncoder::ffmpegWorker, this));

    // Monitor Window
    // TODO: An option to disable this
    namedWindow(windowName, WINDOW_AUTOSIZE );
}

#endif /* __ENCODER_CPP__ */