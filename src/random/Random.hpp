#pragma once

#include <TgBotRandomExports.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "CStringLifetime.h"
#include "InstanceClassBase.hpp"
#include "initcalls/Initcall.hpp"

class TgBotRandom_API Random : public InstanceClassBase<Random>,
                               public InitCall {
   public:
    // Retval type for random
    using ret_type = size_t;
    static_assert(std::is_integral_v<ret_type>);

    /**
     * @brief      Base class for random number generators.
     *
     * This class provides an interface for random number generators. It defines
     * functions for generating random numbers and shuffling containers. The
     * isSupported() function can be used to determine if a particular RNG is
     * available on the system.
     *
     * @tparam     random_return_type  The type of random numbers generated by
     * the RNG.
     */
    struct ImplBase {
        /**
         * @brief      Determines if the RNG is isSupported on the system.
         *
         * @return     `true` if the RNG is isSupported, `false` otherwise.
         */
        virtual bool isSupported() const = 0;

        /**
         * @brief      Generates a random number in the specified range.
         *
         * @param[in]  min  The minimum value of the random number.
         * @param[in]  max  The maximum value of the random number.
         *
         * @return     A random number in the specified range.
         */
        virtual ret_type generate(const ret_type min,
                                  const ret_type max) const = 0;

        /**
         * @brief      Shuffles the elements in a string container.
         *
         * This function uses the random number generator to rearrange the
         * elements in a container. The specific algorithm used for shuffling is
         * dependent on the RNG implementation.
         */
        virtual void shuffle(std::vector<std::string>& it) const = 0;

        /**
         * @brief      Returns the name of the RNG.
         *
         * @return     A string containing the name of the RNG.
         */
        virtual std::string_view getName() const = 0;

        virtual ~ImplBase() = default;

       protected:
       
        /**
         * @brief      Shuffles the elements in a container using the specified
         * random number engine.
         *
         * This function uses the specified random number engine to rearrange
         * the elements in a container. The specific algorithm used for
         * shuffling is dependent on the RNG implementation.
         *
         * @tparam     Engine  The type of random number engine to be used.
         * @tparam     T  The type of elements in the container.
         * @param[in,out]  in  The container to be shuffled.
         * @param[in]  e  The random number engine to be used for shuffling.
         */
        template <class Engine, typename T>
        void ShuffleImpl(std::vector<T>& in, Engine* e) const {
            std::shuffle(in.begin(), in.end(), *e);
        }

        /**
         * @brief      Generates a random number in the specified range using
         * the specified random number generator.
         *
         * This function uses the specified random number generator to generate
         * a random number in the specified range.
         *
         * @tparam     Generator  The type of random number generator to be
         * used.
         * @param[in]  gen  The random number generator to be used for
         * generating the random number.
         * @param[in]  min  The minimum value of the random number.
         * @param[in]  max  The maximum value of the random number.
         *
         * @return     A random number in the specified range.
         */
        template <class Generator>
        ret_type gen_impl(Generator* gen, ret_type min, ret_type max) const {
            if (min > max) {
                LOG(WARNING)
                    << "min(" << min << ") is bigger than max(" << max << ")";
                std::swap(min, max);
            } else if (min == max) {
                LOG(WARNING) << "min == max == " << min;
                return min;
            }
            std::uniform_int_distribution<ret_type> distribution(min, max);
            return distribution(*gen);
        }
    };

    explicit Random(std::unique_ptr<ImplBase> impl) : impl_(std::move(impl)){};

    /**
     * generate - Generate random number given a range.
     * Conditionally uses platform-specific RNG.
     *
     * @param min min value
     * @param max max value
     * @throws std::runtime_error if min >= max
     * @return Generated number
     */
    ret_type generate(const ret_type min, const ret_type max);

    /**
     * Alias for genRandomNumber(int, int) with min parameter as 0
     * @param max max value
     *
     * @throws std::runtime_error if min >= max
     * @return Generated number
     */
    ret_type generate(const ret_type max);

    /**
     * Specialization of shuffleArray for std::string type.
     * Calls an external function shuffleStringArray to shuffle the vector of
     * strings.
     *
     * @param in The vector of strings to be shuffled.
     *
     * @note This function is called only when the template parameter Elem is
     * std::string. It calls an external function shuffleStringArray.
     *
     * @throws No exceptions are thrown by this function.
     */
    void shuffleArray(std::vector<std::string>& inArray);

    void doInitCall() override;

    const CStringLifetime getInitCallName() const override {
        return "Choose random number generator impl";
    }

    [[nodiscard]] ImplBase* getImpl() const {
        return impl_.get();
    }

   private:
    std::unique_ptr<ImplBase> impl_;
};