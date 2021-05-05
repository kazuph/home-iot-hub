#ifndef SERVICE_TRANSFORM_HPP
#define SERVICE_TRANSFORM_HPP

#include "service_traits.hpp"

#include <type_traits>
#include <functional>

namespace hub::service
{
    namespace impl
    {
        template<typename SenderT, typename TransformT>
        class transform
        {
        public:

            using in_message_t  = typename SenderT::out_message_t;
            using out_message_t = std::invoke_result_t<TransformT, in_message_t>;
            using function_type = std::function<void(out_message_t&&)>;

            static_assert(has_output_message_type_v<SenderT>, "SenderT is not a valid message source.");
            static_assert(std::is_invocable_v<TransformT, in_message_t>, "Specified callable is not invocable with sender message type.");

            transform() = delete;

            transform(SenderT&& sender, TransformT transformation) :
                m_sender(std::move(sender)),
                m_transformation(transformation)
            {
                
            }

            template<typename MessageHandlerT>
            void set_message_handler(MessageHandlerT message_handler)
            {
                m_message_handler = message_handler;

                m_sender.set_message_handler([this](in_message_t&& message) {
                    process_message(std::move(message));
                });
            }

            void process_message(in_message_t&& message) const
            {
                m_message_handler(std::invoke(m_transformation, std::move(message)));
            }

        private:

            SenderT         m_sender;
            TransformT      m_transformation;
            function_type   m_message_handler;
        };

        template<typename FunctionT>
        struct transform_helper
        {
            FunctionT m_function;
        };
    }

    template<typename SenderT, typename TransformT>
    inline auto transform(SenderT&& sender, TransformT&& transformation)
    {
        return impl::transform<SenderT, TransformT>(std::forward<SenderT>(sender), std::forward<TransformT>(transformation));
    }

    namespace operators
    {
        template<typename TransformT>
        inline auto transform(TransformT&& transformation)
        {
            return impl::transform_helper<TransformT>{ std::forward<TransformT>(transformation) };
        }

        template<typename SenderT, typename TransformT>
        inline auto operator|(SenderT&& sender, impl::transform_helper<TransformT> transform)
        {
            return impl::transform<SenderT, TransformT>(std::forward<SenderT>(sender), transform.m_function);
        }
    }
}

#endif