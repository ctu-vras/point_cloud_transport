//
// Created by jakub on 7/25/19.
//

#include "point_cloud_transport/subscriber.h"
#include "point_cloud_transport/subscriber_plugin.h"
#include <ros/names.h>
#include <pluginlib/class_loader.h>
#include <boost/scoped_ptr.hpp>

namespace point_cloud_transport {

    struct Subscriber::Impl
    {
        Impl()
                : unsubscribed_(false)
        {
        }

        ~Impl()
        {
            shutdown();
        }

        bool isValid() const
        {
            return !unsubscribed_;
        }

        void shutdown()
        {
            if (!unsubscribed_) {
                unsubscribed_ = true;
                if (subscriber_)
                    subscriber_->shutdown();
            }
        }

        SubLoaderPtr loader_;
        boost::shared_ptr<SubscriberPlugin> subscriber_;
        bool unsubscribed_;
        //double constructed_;
    };

    Subscriber::Subscriber(ros::NodeHandle& nh, const std::string& base_topic, uint32_t queue_size,
                           const boost::function<void(const sensor_msgs::PointCloud2ConstPtr&)>& callback,
                           const ros::VoidPtr& tracked_object, const TransportHints& transport_hints,
                           const SubLoaderPtr& loader)
            : impl_(new Impl)
    {
        // Load the plugin for the chosen transport.
        std::string lookup_name = SubscriberPlugin::getLookupName(transport_hints.getTransport());
        try {
            impl_->subscriber_ = loader->createInstance(lookup_name);
        }
        catch (pluginlib::PluginlibException& e) {
            throw TransportLoadException(transport_hints.getTransport(), e.what());
        }
        // Hang on to the class loader so our shared library doesn't disappear from under us.
        impl_->loader_ = loader;

        // Try to catch if user passed in a transport-specific topic as base_topic.
        std::string clean_topic = ros::names::clean(base_topic);
        size_t found = clean_topic.rfind('/');
        if (found != std::string::npos) {
            std::string transport = clean_topic.substr(found+1);
            std::string plugin_name = SubscriberPlugin::getLookupName(transport);
            std::vector<std::string> plugins = loader->getDeclaredClasses();
            if (std::find(plugins.begin(), plugins.end(), plugin_name) != plugins.end()) {
                std::string real_base_topic = clean_topic.substr(0, found);
                ROS_WARN("[point_cloud_transport] It looks like you are trying to subscribe directly to a "
                         "transport-specific point cloud topic '%s', in which case you will likely get a connection "
                         "error. Try subscribing to the base topic '%s' instead with parameter ~point_cloud_transport "
                         "set to '%s' (on the command line, _point_cloud_transport:=%s). ",
                         clean_topic.c_str(), real_base_topic.c_str(), transport.c_str(), transport.c_str());
            }
        }

        // Tell plugin to subscribe.
        impl_->subscriber_->subscribe(nh, base_topic, queue_size, callback, tracked_object, transport_hints);
    }

    std::string Subscriber::getTopic() const
    {
        if (impl_) return impl_->subscriber_->getTopic();
        return std::string();
    }

    uint32_t Subscriber::getNumPublishers() const
    {
        if (impl_) return impl_->subscriber_->getNumPublishers();
        return 0;
    }

    std::string Subscriber::getTransport() const
    {
        if (impl_) return impl_->subscriber_->getTransportName();
        return std::string();
    }

    void Subscriber::shutdown()
    {
        if (impl_) impl_->shutdown();
    }

    Subscriber::operator void*() const
    {
        return (impl_ && impl_->isValid()) ? (void*)1 : (void*)0;
    }

} //namespace point_cloud_transport

