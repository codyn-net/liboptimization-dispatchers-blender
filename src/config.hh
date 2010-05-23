#ifndef __BLENDER_CONFIG_H__
#define __BLENDER_CONFIG_H__

#include <jessevdk/base/config.hh>
#include <glibmm.h>

namespace blender
{
	class Config : public jessevdk::base::Config
	{
		static Config *s_instance;

		public:
			bool Secure;
			Glib::ustring AllowedOwners;
			Glib::ustring BlenderPath;

			/* Constructor/destructor */
			static Config &Initialize(std::string const &filename);
			static Config &Instance();
		
			/* Public functions */
		private:
			/* Private functions */
			Config();
	};
}

#endif /* __BLENDER_CONFIG_H__ */
