#include "CaffeInference.hpp"
#include <caffe/util/hdf5.hpp>

#include <opencv/cv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;

using caffe::Caffe;
using caffe::Blob;
using caffe::Net;
using caffe::BlobProto;
using caffe::NetParameter;
using caffe::LayerParameter;
using caffe::TransformationParameter;
using caffe::DataTransformer;


CaffeInference::CaffeInference( const string &model_file, const string &trained_file ) : imageBlobName("images"), stateBlobName("states"), outputBlobName("output")
{
    Caffe::set_mode(Caffe::GPU);

    caffeNet.reset(new Net<float>( model_file, caffe::TEST ));
    caffeNet->CopyTrainedLayersFrom(trained_file);

    printNetInfo();

    getTransformation( model_file );
}


void CaffeInference::getTransformation( const string &model_file )
{
    NetParameter net_parameters;
    ReadProtoFromTextFileOrDie( model_file, &net_parameters );
    LayerParameter layer_parameters = net_parameters.layer(0);
    if( layer_parameters.has_transform_param() ){
        TransformationParameter transformation_params = layer_parameters.transform_param();
        dataTransformer.reset( new DataTransformer<float>( transformation_params, caffe::TEST ) );
    }
}


void CaffeInference::printNetInfo() const
{
    cout << "\nNetwork name = " << caffeNet->name() << endl;
    cout << endl;

    vector<string> layers = caffeNet->layer_names();
    cout << "Number of layers = " << layers.size() << endl;
    for(vector<string>::iterator it = layers.begin(); it != layers.end(); ++it)
        cout << "Layer name = " << *it << endl;
    cout << endl;

    vector<string> blobs = caffeNet->blob_names();
    cout << "Number of blobs = " << blobs.size() << endl;
    for(vector<string>::iterator it = blobs.begin(); it != blobs.end(); ++it)
        cout << "Blob name = " << *it << endl;
    cout << endl;
}


void CaffeInference::printBlobInfo( const string &blob_name, const Blob<float> &blob ) const
{
    cout << "\nBlob: " << blob_name << endl;
    cout << "Number of axis = " << blob.num_axes() << endl;
    cout << "Elements count = " << blob.count() << endl;
    for( size_t i = 0; i < blob.num_axes(); ++i )
        cout << "Axis = "<< i << " num elements = " << blob.shape(i) << endl;

    cout << endl;
}


void CaffeInference::printBlobContent( const string &blob_name, Blob<float> &blob ) const
{
    cout << "Blob: " << blob_name << endl;
    const float *content = blob.cpu_data();
    for( size_t i = 0; i < blob.count(); ++i )
        cout << i << " = " << content[i] << endl;
}


void CaffeInference::loadImageMean( const string &mean_file )
{
    BlobProto mean_proto;
    ReadProtoFromBinaryFileOrDie( mean_file.c_str(), &mean_proto );

    Blob<float> mean_blob;
    mean_blob.FromProto(mean_proto);

    imageMean.CopyFrom(mean_blob, false, true);
}


void CaffeInference::transformInput()
{
    Blob<float> input_blob, output_blob;
    dataTransformer->Transform( &input_blob, &output_blob );
}


// loadHDF5 can be used to open the train_policy and train_states files
// Load hdf5 files with ONLY ONE GROUP
void CaffeInference::loadHDF5( const string &file_name, unsigned max_dim )
{
    Blob<float> output_blob;

    hid_t file_hid = H5Fopen( file_name.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    CHECK_GE(file_hid, 0) << "Couldn't open " << file_name;

    string source_layer_name = caffe::hdf5_get_name_by_idx(file_hid, 0);
    cout << "source_layer_name = " << source_layer_name << endl;

    hdf5_load_nd_dataset( file_hid, source_layer_name.c_str(), 0, max_dim, &output_blob);

    printBlobInfo( source_layer_name, output_blob );

    H5Fclose(file_hid);
}


void CaffeInference::feedImage( cv::Mat input_image )
{
    // Crop image and remove mean

    boost::shared_ptr<Blob<float> > image_blob = caffeNet->blob_by_name( imageBlobName );
    printBlobInfo("image_blob", *image_blob);

    cv::Mat image(image_blob->shape(2), image_blob->shape(3), CV_32FC3, image_blob->mutable_cpu_data() );
    image = input_image / 256.0f;
}


void CaffeInference::feedState( const float *input_state )
{
    const unsigned num_elements = sizeof( input_state ) / sizeof( input_state[0] );

    boost::shared_ptr<Blob<float> > state_blob = caffeNet->blob_by_name( stateBlobName );
    printBlobInfo("state_blob", *state_blob);

    // Load state blob with data;
    float *states = state_blob->mutable_cpu_data();
    for(size_t i = 0; i < num_elements; ++i){
        states[i] = input_state[i];
    }
}


void CaffeInference::printOutput() const
{
    boost::shared_ptr<Blob<float> > output_blob = caffeNet->blob_by_name( outputBlobName );
    printBlobInfo("output_blob", *output_blob);

    const float *output_data = output_blob->cpu_data();
    for(size_t i = 0; i < output_blob->shape(1); ++i){
        cout << "Output " << i << " = " << output_data[i] << endl;
    }
}


void CaffeInference::Predict( cv::Mat input_image, const float *input_state )
{
    feedImage( input_image );
    feedState( input_state );

    caffeNet->Forward();

    printOutput();
}
