#include "eir.h"

#include "handler.h"

using namespace std::tr1::placeholders;
using namespace eir;

struct Modloader : public CommandHandlerBase<Modloader>
{
    eir::CommandRegistry::id modload_id, modunload_id, modreload_id;

    void do_modload(const eir::Message *m)
    {
        if(m->source.type != sourceinfo::ConfigFile &&
                !(m->source.client && m->source.client->privs().has_privilege("admin")))
            return;

        Context ctx("Processing MODLOAD " + m->args[0]);

        try
        {
            ModuleRegistry::get_instance()->load(m->args[0]);
            m->source.reply("Loaded " + m->args[0]);
        }
        catch(ModuleError &e)
        {
            m->source.reply("Failed to load " + m->args[0] + ": " + e.message());
            if(m->source.type == sourceinfo::ConfigFile)
                throw;
        }
    }

    void do_modunload(const eir::Message *m)
    {
        if(m->source.type != sourceinfo::ConfigFile &&
                !(m->source.client && m->source.client->privs().has_privilege("admin")))
            return;

        Context ctx("Processing MODUNLOAD " + m->args[0]);

        try
        {
            ModuleRegistry::get_instance()->unload(m->args[0]);
            m->source.reply("Unloaded " + m->args[0]);
        }
        catch(ModuleError &e)
        {
            m->source.reply("Failed to unload " + m->args[0] + ": " + e.message());
            if(m->source.type == sourceinfo::ConfigFile)
                throw;
        }
     }

    void do_modreload(const eir::Message *m)
    {
        if(m->source.type != sourceinfo::ConfigFile &&
                !(m->source.client && m->source.client->privs().has_privilege("admin")))
            return;

        Context ctx("Processing MODRELOAD " + m->args[0]);

        if (ModuleRegistry::get_instance()->is_loaded(m->args[0]))
        {
            try
            {
                ModuleRegistry::get_instance()->unload(m->args[0]);
                m->source.reply("Unloaded " + m->args[0]);
            }
            catch(ModuleError &e)
            {
                m->source.reply("Failed to unload " + m->args[0] + ": " + e.message());
                if(m->source.type == sourceinfo::ConfigFile)
                    throw;
                return;
            }
        }
        try
        {
            ModuleRegistry::get_instance()->load(m->args[0]);
            m->source.reply("Loaded " + m->args[0]);
        }
        catch(ModuleError &e)
        {
            m->source.reply("Failed to load " + m->args[0] + ": " + e.message());
            if(m->source.type == sourceinfo::ConfigFile)
                throw;
        }
     }

    Modloader() {
        modload_id = add_handler("modload", &Modloader::do_modload);
        modunload_id = add_handler("modunload", &Modloader::do_modunload);
        modreload_id = add_handler("modreload", &Modloader::do_modreload);
    }

    ~Modloader() {
        remove_handler(modload_id);
        remove_handler(modunload_id);
        remove_handler(modreload_id);
    }
};

Modloader m;
