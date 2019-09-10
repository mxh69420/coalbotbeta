/* speeds up compilation by like 5 seconds
 * so i guess thats a good thing
*/

#define BOOST_ASIO_SEPARATE_COMPILATION
#define BOOST_BEAST_SEPARATE_COMPILATION

#include <boost/asio/impl/src.hpp>
#include <boost/asio/ssl/impl/src.hpp>
#include <boost/beast/src.hpp>

#include <nlohmann/json.hpp>
template class nlohmann::basic_json<>;
