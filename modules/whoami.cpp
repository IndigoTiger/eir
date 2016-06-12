#include "eir.h"

#include <functional>

using namespace eir;

struct Whoami : CommandHandlerBase<Whoami>, Module
{
    void whoami(const Message *m)
    {
        if (!m->source.client)
        {
            m->source.reply("I don't know who you are.");
            return;
        }

        std::string whobuf;
        whobuf.append("You are " + m->source.client->nick());

        if (m->source.client->account().empty())
            whobuf.append(" and you are not logged in");
        else
            whobuf.append(" and you are logged in as " + m->source.client->account());

        m->source.reply(whobuf);

        std::string privbuf;
        std::map<std::string, std::string> chanprivbuf;

        for(PrivilegeSet::iterator it = m->source.client->privs().begin();
                it != m->source.client->privs().end(); ++it)
        {
            if (it->first.empty())
                privbuf += it->second + " ";
            else
                chanprivbuf[it->first] += it->second + " ";
        }

        if (!privbuf.empty())
            m->source.reply("You have privileges " + privbuf);
        for (std::map<std::string,std::string>::iterator it = chanprivbuf.begin(); it != chanprivbuf.end(); ++it)
            m->source.reply("You have privileges " + it->second + "in channel " + it->first);
    }

    CommandHolder _id;

    Whoami()
    {
        _id = add_handler(filter_command_type("whoami", sourceinfo::IrcCommand), &Whoami::whoami);
    }
};

MODULE_CLASS(Whoami)
