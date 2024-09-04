#include <iostream>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <deque>
#include <fstream>
#include <unordered_map>
#include <csignal>
#include <limits>
#include <opencv2/opencv.hpp>
#include "mixer_processor.h"

// #include <pthread.h>

#include "comms.h"

#define APPLY_LOW_PASS_FILTER true // low pass filter the noise Set to false to disable low-pass filtering

#define NUM_OF_NOISE_FRAMES 30

#define FULLSCREEN_MODE false // Set to false for windowed mode

#define SHOW_TIMING true // Show timing on screen

// #define NOISE_WEIGHT .6 // 0.5 // Adjust this value as needed (e.g., 0.33 for 1/3)

// #define OUTPUT_GAIN 1.8 // Adjust this value for output gain of the final image

// #define FADE_TIMER_TC 64 // Adjust this value for lenngth of fade

// #define FADE_TIME 38 // Adjust this value for lenngth of fade  nominal 38 frames

struct Server_Parameters_Main
{
    int Screen_H_Size;      //  in pixels
    int Screen_V_Size;      //  in pixels
    int Noise_Gain;         // in percent
    int Input_Gain;         // in percent
    int Output_Gain;        // in percent
    int Gamma_Gain;         // in percent
    int Cycle_Time;         // cycle time determined by the images coming in so not really used
    int Fade_Time;          // in frames 1/30 of a second
    int Full_Screen_Enable; // in frames 1/30 of a second

    // Constructor to initialize default values
    Server_Parameters_Main() : Screen_H_Size(1024), Screen_V_Size(768),
                               Noise_Gain(60), Input_Gain(75), Output_Gain(180), Gamma_Gain(100), Cycle_Time(64), Fade_Time(38), Full_Screen_Enable(0) {}
};

// std::map<std::string, int> parseString(const std::string& input_string,  Server_Parameters_Main & Server_Param_Local) {
void parseString(const std::string &input_string, Server_Parameters_Main &params)
{
    std::istringstream iss(input_string);
    std::map<std::string, int> parsed_values;
    std::string key;
    int value;
    static bool First_Time = true;

    // Parse the string
    while (iss >> key >> value)
    {
        parsed_values[key] = value;
    }

    //  names_to_send_4.push_back("Scrn_H 1024  Scrn_V 768 Noise_Gn 14 In_Gn 2200  Out_Gn 768  Gma_Gn 255  Cycle_Tme  165 Fade_Tme 190");
    params.Screen_H_Size = parsed_values["Scrn_H"];
    params.Screen_V_Size = parsed_values["Scrn_V"];
    params.Noise_Gain = parsed_values["Noise_Gn"];
    params.Input_Gain = parsed_values["In_Gn"];
    params.Output_Gain = parsed_values["Out_Gn"];
    params.Gamma_Gain = parsed_values["Gma_Gn"];
    params.Cycle_Time = parsed_values["Cycle_Tme"];
    params.Fade_Time = parsed_values["Fade_Tme"];
    params.Full_Screen_Enable = parsed_values["Full_Screen_Enable"];

    if (First_Time)
    {
        std::cout << "Parameter 0: " << params.Screen_H_Size << std::endl;
        std::cout << "Parameter 1: " << params.Screen_V_Size << std::endl;
        std::cout << "Parameter 2: " << params.Noise_Gain << std::endl;
        std::cout << "Parameter 3: " << params.Input_Gain << std::endl;
        std::cout << "Parameter 4: " << params.Output_Gain << std::endl;
        std::cout << "Parameter 5: " << params.Gamma_Gain << std::endl;
        std::cout << "Parameter 6: " << params.Cycle_Time << std::endl;
        std::cout << "Parameter 7: " << params.Fade_Time << std::endl;
        std::cout << "Parameter 8: " << params.Full_Screen_Enable << std::endl;
        First_Time = false;
    }
}

void usage()
{
    cout << "Sample MRR_Pi server code handling display and image messages." << endl;
    cout << "Each server instance is meant to be paired with a single instance of MRR_Pi_client." << endl;
    cout << endl;
    cout << "usage: MRR_Pi_server" << endl;
    cout << "  [-p port number, range 1024 to 49151, default = " << Comm::default_port << " ]" << endl;
    cout << endl;

    cout << "sample command line (runs server on the default port): ./MRR_Pi_server" << endl;
    cout << "sample command line (specifies port): ./MRR_Pi_server -p 5577" << endl;
    cout << endl;
}

int main(int argc, char *argv[])
{

    Server_Parameters_Main Server_Params;
    string control_params_in;

    int Fade_Timer = 0;
    // int Fade_Timer_TC = 64; //  at  30 fps  64/30 seconds
    // int Fade_Time = 38;
    bool New_Image = false;
    float Fade_Val = 0;

    float avg_sum = 0;
    int average_cnter = 0;

    // create a gradient for test image using a pointer
    int width = 1024;
    int height = 768;
    int size = width * height;
    uchar *dataX = new uchar[size];
    for (int i = 0; i < size; ++i)
    {
        dataX[i] = i / 3072; // Example: gradient effect
    }

    // use memcopy to convert Jonathan's container to an opencv Mat   // had ame offset reults
    cv::Mat image1(height, width, CV_8UC1); // Create an empty cv::Mat with the desired dimensions
    cv::Mat image2(height, width, CV_8UC1); // Create an empty cv::Mat with the desired dimensions

    cv::Mat transformedImg(height, width, CV_8UC1); // Create an empty cv::Mat with the desired dimensions

    // for timing various things
    auto start_check = std::chrono::high_resolution_clock::now();
    auto end_check = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_check - start_check;

    auto start_check_2 = std::chrono::high_resolution_clock::now();
    auto end_check_2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_2 = end_check_2 - start_check_2;

    usage();

    double fps = 30;

    Comm *comm = Comm::start_server(nullptr, argc, argv);
    if (comm == nullptr)
    {
        return -1;
    }

    // for debugging
    // string files[] = {
    //     "../raw/24-06-03-04-30-10.raw",
    //     "../raw/24-06-03-04-31-09.raw",
    //     "../raw/24-06-03-04-35-02.raw",
    //     "../raw/24-06-03-04-32-06.raw",
    //     "../raw/24-06-03-04-37-06.raw"};

    // // cache image data for later checking against incoming image data
    // std::unordered_map<std::string, string> file_strings;
    // for (auto &filename : files)
    // {
    //     file_strings[filename] = load_image(filename);
    // }
    // end debugging

    long image_count = 0;
    long matched_count = 0;
    long mismatched_count = 0;
    auto begin = SteadyClock::now();

    long max_loop = std::numeric_limits<long>::max();

    SD loop_sd;
    deque<MessageData *> cached_messages;

    // generate noise
    std::vector<cv::Mat> noiseFrames = generateNoiseFrames(image1.cols, image1.rows, NUM_OF_NOISE_FRAMES, APPLY_LOW_PASS_FILTER);
    //  Create the parabolic lookup table for gamma correction
    cv::Mat lut = createParabolicLUT();




    cv::namedWindow("Grayscale Image 3", cv::WINDOW_NORMAL);


    for (long loop_count = 0; loop_count < max_loop; loop_count++)
    {

        if (Server_Params.Full_Screen_Enable == 1)
        {

            cv::setWindowProperty("Grayscale Image 3", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN); // Set window to fullscreen
            cout << " HERE " ;
        }

        
        deque<MessageData *> to_delete;
        deque<MessageData *> received_messages;
        while (auto message_data = comm->next_received())
        {
            received_messages.push_back(message_data);
        }

        // loop here isn't strictly necessary, since images will probably arrive one at a time
        for (auto message_data : received_messages)
        {
            bool do_delete = true; // delete messages that don't contain images
            if (message_data->message_type == MessageData::MessageType::IMAGE)
            {
                do_delete = false;
                cached_messages.push_back(message_data);

                // for debugging
                cout << "got image '" << message_data->image_name << "' sz:" << message_data->image_data.size() << endl;
                control_params_in = message_data->image_name;

                New_Image = true;

                image_count += 1;

                // for (auto filename : files)
                // {
                //     if (message_data->image_name.find(filename) != string::npos)
                //     {
                //         if (message_data->image_data == file_strings[filename])
                //         {
                //             matched_count += 1;
                //         }
                //         else
                //         {
                //             mismatched_count += 1;
                //         }
                //         break;
                //     }
                // }
                // end debugging
            }

            if (do_delete)
            {
                to_delete.push_back(message_data);
            }
        }

        while (cached_messages.size() > 2)
        {
            to_delete.push_back(cached_messages.front());
            cached_messages.pop_front();
        }

        // delete unwanted messages
        for (auto message_data : to_delete)
        {
            cout << "deleting ty:" << message_data->message_type << " " << message_data->image_name << endl;
            delete message_data;
        }

        if (New_Image)
        {

            // cout << " BIG NAME " << control_params_in << endl;

            parseString(control_params_in, Server_Params);

            cout << "HELLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL  " << Server_Params.Noise_Gain;

            // exit(0);

            Fade_Timer = 0;
            // image2 = image1.clone();
            memcpy(image2.data, cached_messages[0]->image_data.c_str(), size * sizeof(uchar));
            if (cached_messages.size() > 1)
            {
                memcpy(image1.data, cached_messages[1]->image_data.c_str(), size * sizeof(uchar));
            }
            New_Image = false;
            Fade_Val = 0;
        }
        else if (Fade_Timer < Server_Params.Cycle_Time)
        {
            Fade_Timer++;
            Fade_Val = (float)(Fade_Timer <= Server_Params.Fade_Time ? Fade_Timer : Server_Params.Fade_Time) / (float)Server_Params.Fade_Time;
        }

        // std::cout << "Parameter 0: " << params.Screen_H_Size << std::endl;
        // std::cout << "Parameter 1: " << params.Screen_V_Size << std::endl;
        // std::cout << "Parameter 2: " << params.Noise_Gain << std::endl;
        // std::cout << "Parameter 3: " << params.Input_Gain << std::endl;
        // std::cout << "Parameter 4: " << params.Output_Gain << std::endl;
        // std::cout << "Parameter 5: " << params.Gamma_Gain << std::endl;
        // std::cout << "Parameter 6: " << params.Cycle_Time << std::endl;
        // std::cout << "Parameter 7: " << params.Fade_Time << std::endl;

        // cout << "  Fade_Val  "  << Server_Params.Fade_Time    << " gn "  <<  ( (float)Server_Params.Output_Gain)/100.   << "  "  << ( (float)Server_Params.Noise_Gain) / 100.   <<  "  "  <<   OUTPUT_GAIN  <<  endl ;

        // float img2Fade = 1.0f - img1Fade;

        blendImagesAndNoise(image1, image2, noiseFrames, transformedImg, lut, Fade_Val,
                            (float)Server_Params.Input_Gain / 100,
                            (float)Server_Params.Noise_Gain / 100,
                            (float)Server_Params.Gamma_Gain / 100,
                            (float)Server_Params.Output_Gain / 100);

        // blendImagesAndNoise(image1, image2, noiseFrames, transformedImg, lut, Server_Params.Fade_Time, (float)Server_Params.Noise_Gain / 100 , 1.8) ; // (float)Server_Params.Output_Gain/100  );

        end_check_2 = std::chrono::high_resolution_clock::now();
        elapsed_2 = end_check_2 - start_check_2;
        average_cnter++;
        if (average_cnter >= 30)
        {
            // cout << "elapsed_2: " << elapsed_2.count() << endl;
            cout << "elapsed_2: " << avg_sum / 30 << endl;
            avg_sum = elapsed_2.count();
            average_cnter = 0;
        }
        else
            avg_sum += elapsed_2.count();

        // Loop Timer to set frame rate
        double goal = (loop_count + 1) / fps;
        Seconds elapsed = SteadyClock::now() - begin;
        if (elapsed.count() < goal)
        {
            this_thread::sleep_for(std::chrono::duration<double>(goal - elapsed.count()));
        }

        start_check_2 = std::chrono::high_resolution_clock::now();

        // display images code here
        // Display the image
        cv::imshow("Grayscale Image 3", transformedImg);
        // cv::imshow("Grayscale Image 3", image_mixed);

        // needed for opencv loop
        int key = cv::waitKey(1);
        if (key == 27)
        { // ASCII code for the escape key
            break;
        }

        // check for long frame times
        end_check = std::chrono::high_resolution_clock::now();
        elapsed = end_check - start_check;
        start_check = std::chrono::high_resolution_clock::now();
        if (elapsed.count() > .04)
            cout << "XXXXXXXXXXXXXXXXXX  " << elapsed.count() << endl;

        // for debugging
        auto current = SteadyClock::now();
        loop_sd.increment(current);
        // for debugging
        elapsed = current - begin;
        std::ofstream out("server_counter_2_" + comm->port() + ".txt");
        out << "t:" << elapsed.count() << "s" << endl;
        out << "images_rec'd: " << image_count << endl;
        out << "match: " << matched_count << endl;
        out << "mismatch: " << mismatched_count << endl;
        loop_sd.dump(out, "loop");
        out.close();
        // end debugging
    }

    return 0;
}
