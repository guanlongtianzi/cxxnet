#ifndef CXXNET_LAYER_INL_HPP
#define CXXNET_LAYER_INL_HPP
#pragma once
/*!
 * \file cxxnet_layer-inl.hpp
 * \brief implementation of different layers
 * \author Bing Xu, Tianqi Chen
 */

#include "cxxnet_net.h"
#include "mshadow/tensor_container.h"

namespace cxxnet{
    // expr is needed to use expression
    using namespace mshadow::expr;
    using namespace mshadow::utils;
    
    /*! \brief potential parameters for each layer */
    struct LayerParam{
        /*! \brief initialization sd for weight */
        float init_sigma;
        /*!
         * \brief Set param for the layer from string
         * \param name parameter name
         * \param val string for configuration
         */
        inline void SetParam(const char *name, const char* val) {
            if( !strcmp( name, "init_sigma") ) init_sigma = (float)atof(val);
        }
    };
};

namespace cxxnet {
    // simple fully connected layer that connects two nodes
    template<typename xpu>
    class FullConnectLayer : public ILayer{
    public:
        FullConnectLayer( mshadow::Random<xpu> &rnd, Node<xpu> &in, Node<xpu> &out )
            :rnd_(rnd), in_(in), out_(out){
            Assert( in_.data.shape[1] == out_.data.shape[1], "input output batch mismatch" );
            wmat_.Resize( mshadow::Shape2( in_.data.shape[0], out_.data.shape[0] ) );
            gwmat_.Resize( wmat_.shape );
            bias_.Resize( mshadow::Shape1( out_.data.shape[0] ) );
            gbias_.Resize( gbias_.shape );
        }            
        virtual ~FullConnectLayer( void ){            
        }
        virtual void Forward(bool is_train) {
            index_t nbatch = in_.data.shape[1];
            out_.mat()  = dot( in_.mat(), wmat_ );
            out_.mat() += repmat( bias_, nbatch );
        }
        virtual void Backprop(bool is_firstlayer){
            index_t nbatch = in_.data.shape[1];
            real_t scale = 1.0f / nbatch;            
            // accumulates gradient, instead of set gradient
            gwmat_ += scale * dot( in_.mat().T(), out_.mat() );
            gbias_ += scale * sum_rows( out_.mat() );
            // backprop
            if( is_firstlayer ){
                in_.mat() = dot( out_.mat(), wmat_.T() );
            }
        }
        virtual void GetUpdaters( const char *updater, std::vector<IUpdater*> &updaters ){
            updaters.push_back( CreateUpdater( updater, rnd_, wmat_, gwmat_, "wmat" ) );
            updaters.push_back( CreateUpdater( updater, rnd_, bias_, gbias_, "bias" ) );
        }
        virtual void SetParam(const char *name, const char* val){
            param_.SetParam( name, val );
        }
        virtual void InitModel(void){            
            rnd_.SampleGaussian( wmat_, 0.0f, param_.init_sigma );
            bias_ = 0.0f; gwmat_ = 0.0f; gbias_ = 0.0f;
        }
        virtual void SaveModel(mshadow::utils::IStream &fo) const{
            wmat_.SaveBinary( fo );
            bias_.SaveBinary( fo );
            gwmat_.SaveBinary( fo );
            gbias_.SaveBinary( fo );
        }
        virtual void LoadModel(mshadow::utils::IStream &fi){
            wmat_.LoadBinary( fi );
            bias_.LoadBinary( fi );
            gwmat_.LoadBinary( fi );
            gbias_.LoadBinary( fi );
        }
    private:
        /*! \brief parameters that potentially be useful */
        LayerParam param_;
        /*! \brief random number generator */
        mshadow::Random<xpu> &rnd_;
        /*! \brief input node */
        Node<xpu> &in_; 
        /*! \brief output node */
        Node<xpu> &out_;
        /*! \brief weight matrix */
        mshadow::TensorContainer<xpu,2> wmat_;
        /*! \brief bias */
        mshadow::TensorContainer<xpu,1> bias_;
        /*! \brief accumulates the gradient of weight matrix */
        mshadow::TensorContainer<xpu,2> gwmat_;
        /*! \brief accumulates the gradient of bias */
        mshadow::TensorContainer<xpu,1> gbias_;
    };

    // For softmax, we do not need to store weight/bias, only use softmax as a kinda of transformation
    // we can use full layer -> softmax layer
    template<typename xpu>
    class SoftmaxLayer: public ILayer{
    public:
        SoftmaxLayer( Node<xpu> &in, Node<xpu> &out )
            :out_(out){
            Assert( &in == &out, "BUG" );
        }
        virtual void Forward(bool is_train){
            mshadow::Softmax( out_.mat(), out_.mat() );            
        }
        virtual void Backprop(bool is_firstlayer){
            // TODO, or maybe do nothing, let cxxnet operate on node
            // minus gradient
        }
    private:
        /*! \brief only transform on out */
        Node<xpu> &out_;
    };
}; // namespace cxxnet

namespace cxxnet{
    inline int GetLayerType( const char *type ){
        using namespace layer_type;
        if( !strcmp( type, "fullc") )   return kFullConnect;
        if( !strcmp( type, "softmax") ) return kSoftmax;
        return 0;
    }

    template<typename xpu>
    inline ILayer* CreateLayer( int type, mshadow::Random<xpu> &rnd, Node<xpu> &in, Node<xpu> &out ){
        using namespace layer_type;
        switch( type ){
        case kFullConnect: return new FullConnectLayer<xpu>( rnd, in, out );
        case kSoftmax    : return new SoftmaxLayer<xpu>( in, out );
        default: Error("unknown layer type");
        }
        return NULL;
    }
    template<typename xpu>
    inline ILayer* CreateLayer( const char *type, mshadow::Random<xpu> &rnd, Node<xpu> &in, Node<xpu> &out ){
        return CreateLayer( GetLayerType(type), rnd, in, out );
    }
};
#endif // CXXNET_LAYER_INL_HPP