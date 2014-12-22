#include <rai/qt/qt.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <cryptopp/sha3.h>

#include <sstream>

rai_qt::client::client (QApplication & application_a, rai::client & client_a, rai::uint256_union const & wallet_a) :
client_m (client_a),
password_change (*this),
enter_password (*this),
advanced (*this),
block_creation (*this),
block_entry (*this),
application (application_a),
main_stack (new QStackedWidget),
client_window (new QWidget),
client_layout (new QVBoxLayout),
balance_label (new QLabel),
entry_window (new QWidget),
entry_window_layout (new QVBoxLayout),
wallet_model (new QStandardItemModel),
wallet_view (new QTableView),
send_blocks (new QPushButton ("Send")),
wallet_add_account (new QPushButton ("Create account")),
show_advanced (new QPushButton ("Advanced")),
send_blocks_window (new QWidget),
send_blocks_layout (new QVBoxLayout),
send_account_label (new QLabel ("Destination account:")),
send_account (new QLineEdit),
send_count_label (new QLabel ("Amount:")),
send_count (new QLineEdit),
send_blocks_send (new QPushButton ("Send")),
send_blocks_back (new QPushButton ("Back"))
{
    wallet = client_m.wallets.open (wallet_a);
    if (wallet == nullptr)
    {
        wallet = client_m.wallets.create (wallet_a);
    }
    send_blocks_layout->addWidget (send_account_label);
    send_blocks_layout->addWidget (send_account);
    send_blocks_layout->addWidget (send_count_label);
    send_blocks_layout->addWidget (send_count);
    send_blocks_layout->addWidget (send_blocks_send);
    send_blocks_layout->addStretch ();
    send_blocks_layout->addWidget (send_blocks_back);
    send_blocks_layout->setContentsMargins (0, 0, 0, 0);
    send_blocks_window->setLayout (send_blocks_layout);

	wallet_model->setHorizontalHeaderItem (0, new QStandardItem ("Balance"));
	wallet_model->setHorizontalHeaderItem (1, new QStandardItem ("Account"));
    wallet_view->setEditTriggers (QAbstractItemView::NoEditTriggers);
    wallet_view->setModel (wallet_model);
	wallet_view->horizontalHeader ()->setSectionResizeMode (0, QHeaderView::ResizeMode::ResizeToContents);
	wallet_view->horizontalHeader ()->setSectionResizeMode (1, QHeaderView::ResizeMode::Stretch);
    wallet_view->verticalHeader ()->hide ();
    wallet_view->setContextMenuPolicy (Qt::ContextMenuPolicy::CustomContextMenu);

    entry_window_layout->addWidget (wallet_view);
    entry_window_layout->addWidget (send_blocks);
    entry_window_layout->addWidget (wallet_add_account);
    entry_window_layout->addWidget (show_advanced);
    entry_window_layout->setContentsMargins (0, 0, 0, 0);
    entry_window_layout->setSpacing (5);
    entry_window->setLayout (entry_window_layout);

    main_stack->addWidget (entry_window);

    balance_label->setAlignment (Qt::AlignCenter);
    client_layout->addWidget (balance_label);
    client_layout->addWidget (main_stack);
    client_layout->setSpacing (0);
    client_layout->setContentsMargins (0, 0, 0, 0);
    client_window->setLayout (client_layout);
    client_window->resize (320, 480);

    QObject::connect (wallet_view, &QTableView::clicked, [this] (QModelIndex const & index_a)
    {
        auto item (wallet_model->item (index_a.row (), 1));
        assert (item != nullptr);
        application.clipboard ()->setText (item->text ());
    });
    QObject::connect (show_advanced, &QPushButton::released, [this] ()
    {
        push_main_stack (advanced.window);
    });
    QObject::connect (send_blocks_send, &QPushButton::released, [this] ()
    {
        QString coins_text (send_count->text ());
        std::string coins_text_narrow (coins_text.toLocal8Bit ());
        try
        {
            auto scaled (std::stoull (coins_text_narrow));
            rai::uint128_t coins (rai::scale_up (scaled));
            if (rai::scale_down (coins) == scaled)
            {
                QPalette palette;
                palette.setColor (QPalette::Text, Qt::black);
                send_count->setPalette (palette);
                QString account_text (send_account->text ());
                std::string account_text_narrow (account_text.toLocal8Bit ());
                rai::account account;
                auto parse_error (account.decode_base58check (account_text_narrow));
                if (!parse_error)
                {
                    auto send_error (wallet->send (account, coins));
                    if (!send_error)
                    {
                        QPalette palette;
                        palette.setColor (QPalette::Text, Qt::black);
                        send_account->setPalette (palette);
                        send_count->clear ();
                        send_account->clear ();
                        refresh_wallet ();
                    }
                    else
                    {
                        QPalette palette;
                        palette.setColor (QPalette::Text, Qt::red);
                        send_count->setPalette (palette);
                    }
                }
                else
                {
                    QPalette palette;
                    palette.setColor (QPalette::Text, Qt::red);
                    send_account->setPalette (palette);
                }
            }
            else
            {
                QPalette palette;
                palette.setColor (QPalette::Text, Qt::red);
                send_account->setPalette (palette);
            }
        }
        catch (...)
        {
            QPalette palette;
            palette.setColor (QPalette::Text, Qt::red);
            send_count->setPalette (palette);
        }
    });
    QObject::connect (send_blocks_back, &QPushButton::released, [this] ()
    {
        pop_main_stack ();
    });
    QObject::connect (send_blocks, &QPushButton::released, [this] ()
    {
        push_main_stack (send_blocks_window);
    });
    QObject::connect (wallet_add_account, &QPushButton::released, [this] ()
    {
        rai::keypair key;
        wallet->store.insert (key.prv);
        refresh_wallet ();
    });
    client_m.send_observers.push_back ([this] (rai::send_block const &, rai::account const & account_a, rai::amount const &)
    {
        if (wallet->store.exists (account_a))
        {
            refresh_wallet ();
        }
    });
    client_m.receive_observers.push_back ([this] (rai::receive_block const &, rai::account const & account_a, rai::amount const &)
    {
        if (wallet->store.exists (account_a))
        {
            refresh_wallet ();
        }
    });
    client_m.open_observers.push_back ([this] (rai::open_block const &, rai::account const & account_a, rai::amount const &, rai::account const &)
    {
        if (wallet->store.exists (account_a))
        {
            refresh_wallet ();
        }
    });
    refresh_wallet ();
}

void rai_qt::client::refresh_wallet ()
{
    rai::uint128_t balance;
	wallet_model->removeRows (0, wallet_model->rowCount ());
    for (auto i (wallet->store.begin ()), j (wallet->store.end ()); i != j; ++i)
    {
		QList <QStandardItem *> items;
        std::string account;
        rai::public_key key (i->first);
        auto account_balance (client_m.ledger.account_balance (key));
		balance += account_balance;
        auto balance (std::to_string (rai::scale_down (account_balance)));
		items.push_back (new QStandardItem (balance.c_str ()));
        key.encode_base58check (account);
		items.push_back (new QStandardItem (QString (account.c_str ())));
		wallet_model->appendRow (items);
    }
    balance_label->setText (QString ((std::string ("Balance: ") + std::to_string (rai::scale_down (balance))).c_str ()));
}

rai_qt::client::~client ()
{
}

void rai_qt::client::push_main_stack (QWidget * widget_a)
{
    main_stack->addWidget (widget_a);
    main_stack->setCurrentIndex (main_stack->count () - 1);
}

void rai_qt::client::pop_main_stack ()
{
    main_stack->removeWidget (main_stack->currentWidget ());
}

void rai_qt::password_change::clear ()
{
    password_label->setStyleSheet ("QLabel { color: black }");
    password->clear ();
    retype_label->setStyleSheet ("QLabel { color: black }");
    retype->clear ();
}

rai_qt::password_change::password_change (rai_qt::client & client_a) :
window (new QWidget),
layout (new QVBoxLayout),
password_label (new QLabel ("New password:")),
password (new QLineEdit),
retype_label (new QLabel ("Retype password:")),
retype (new QLineEdit),
change (new QPushButton ("Change password")),
back (new QPushButton ("Back")),
client (client_a)
{
    password->setEchoMode (QLineEdit::EchoMode::Password);
    layout->addWidget (password_label);
    layout->addWidget (password);
    retype->setEchoMode (QLineEdit::EchoMode::Password);
    layout->addWidget (retype_label);
    layout->addWidget (retype);
    layout->addWidget (change);
    layout->addStretch ();
    layout->addWidget (back);
    layout->setContentsMargins (0, 0, 0, 0);
    window->setLayout (layout);
    QObject::connect (change, &QPushButton::released, [this] ()
    {
        if (client.wallet->store.valid_password ())
        {
            if (password->text () == retype->text ())
            {
                client.wallet->store.rekey (std::string (password->text ().toLocal8Bit ()));
                clear ();
            }
            else
            {
                password_label->setStyleSheet ("QLabel { color: red }");
                retype_label->setStyleSheet ("QLabel { color: red }");
            }
        }
    });
    QObject::connect (back, &QPushButton::released, [this] ()
    {
        clear ();
        client.pop_main_stack ();
    });
}

rai_qt::enter_password::enter_password (rai_qt::client & client_a) :
window (new QWidget),
layout (new QVBoxLayout),
valid (new QLabel),
password (new QLineEdit),
unlock (new QPushButton ("Unlock")),
lock (new QPushButton ("Lock")),
back (new QPushButton ("Back")),
client (client_a)
{
    password->setEchoMode (QLineEdit::EchoMode::Password);
    layout->addWidget (valid);
    layout->addWidget (password);
    layout->addWidget (unlock);
    layout->addWidget (lock);
    layout->addStretch ();
    layout->addWidget (back);
    window->setLayout (layout);
    QObject::connect (back, &QPushButton::released, [this] ()
    {
        assert (client.main_stack->currentWidget () == window);
        client.pop_main_stack ();
    });
    QObject::connect (unlock, &QPushButton::released, [this] ()
    {
        client.wallet->store.enter_password (std::string (password->text ().toLocal8Bit ()));
        update_label ();
    });
    QObject::connect (lock, &QPushButton::released, [this] ()
    {
        rai::uint256_union empty;
        empty.clear ();
        client.wallet->store.password.value_set (empty);
        update_label ();
    });
}

void rai_qt::enter_password::activate ()
{
    client.push_main_stack (window);
    update_label ();
}

void rai_qt::enter_password::update_label ()
{
    if (client.wallet->store.valid_password ())
    {
        valid->setStyleSheet ("QLabel { color: black }");
        valid->setText ("Password: Valid");
        password->setText ("");
    }
    else
    {
        valid->setStyleSheet ("QLabel { color: red }");
        valid->setText ("Password: INVALID");
    }
}

rai_qt::advanced_actions::advanced_actions (rai_qt::client & client_a) :
window (new QWidget),
layout (new QVBoxLayout),
enter_password (new QPushButton ("Enter Password")),
change_password (new QPushButton ("Change Password")),
show_ledger (new QPushButton ("Ledger")),
show_peers (new QPushButton ("Peers")),
wallet_key_text (new QLabel ("Account key:")),
wallet_key_line (new QLineEdit),
wallet_add_key_button (new QPushButton ("Add account key")),
search_for_receivables (new QPushButton ("Search for receivables")),
wallet_refresh (new QPushButton ("Refresh Wallet")),
create_block (new QPushButton ("Create Block")),
enter_block (new QPushButton ("Enter Block")),
back (new QPushButton ("Back")),
ledger_window (new QWidget),
ledger_layout (new QVBoxLayout),
ledger_model (new QStandardItemModel),
ledger_view (new QTableView),
ledger_refresh (new QPushButton ("Refresh")),
ledger_back (new QPushButton ("Back")),
peers_window (new QWidget),
peers_layout (new QVBoxLayout),
peers_model (new QStringListModel),
peers_view (new QListView),
peers_refresh (new QPushButton ("Refresh")),
peers_back (new QPushButton ("Back")),
client (client_a)
{
    ledger_model->setHorizontalHeaderItem (0, new QStandardItem ("Account"));
    ledger_model->setHorizontalHeaderItem (1, new QStandardItem ("Balance"));
    ledger_model->setHorizontalHeaderItem (2, new QStandardItem ("Block"));
    ledger_view->setModel (ledger_model);
    ledger_view->setEditTriggers (QAbstractItemView::NoEditTriggers);
    ledger_view->horizontalHeader ()->setSectionResizeMode (0, QHeaderView::ResizeMode::Stretch);
    ledger_view->horizontalHeader ()->setSectionResizeMode (1, QHeaderView::ResizeMode::ResizeToContents);
    ledger_view->horizontalHeader ()->setSectionResizeMode (2, QHeaderView::ResizeMode::Stretch);
    ledger_view->verticalHeader ()->hide ();
    ledger_layout->addWidget (ledger_view);
    ledger_layout->addWidget (ledger_refresh);
    ledger_layout->addWidget (ledger_back);
    ledger_layout->setContentsMargins (0, 0, 0, 0);
    ledger_window->setLayout (ledger_layout);
    
    peers_view->setEditTriggers (QAbstractItemView::NoEditTriggers);
    peers_view->setModel (peers_model);
    peers_layout->addWidget (peers_view);
    peers_layout->addWidget (peers_refresh);
    peers_layout->addWidget (peers_back);
    peers_layout->setContentsMargins (0, 0, 0, 0);
    peers_window->setLayout (peers_layout);

    layout->addWidget (enter_password);
    layout->addWidget (change_password);
    layout->addWidget (show_ledger);
    layout->addWidget (show_peers);
    layout->addWidget (wallet_key_text);
    layout->addWidget (wallet_key_line);
    layout->addWidget (wallet_add_key_button);
    layout->addWidget (search_for_receivables);
    layout->addWidget (wallet_refresh);
    layout->addWidget (create_block);
    layout->addWidget (enter_block);
    layout->addStretch ();
    layout->addWidget (back);
    window->setLayout (layout);

    QObject::connect (enter_password, &QPushButton::released, [this] ()
    {
        client.enter_password.activate ();
    });
    QObject::connect (change_password, &QPushButton::released, [this] ()
    {
        client.push_main_stack (client.password_change.window);
    });
    QObject::connect (wallet_refresh, &QPushButton::released, [this] ()
    {
        client.refresh_wallet ();
    });
    QObject::connect (show_peers, &QPushButton::released, [this] ()
    {
        client.push_main_stack (peers_window);
    });
    QObject::connect (show_ledger, &QPushButton::released, [this] ()
    {
        client.push_main_stack (ledger_window);
    });
    QObject::connect (back, &QPushButton::released, [this] ()
    {
        client.pop_main_stack ();
    });
    QObject::connect (peers_back, &QPushButton::released, [this] ()
    {
        client.pop_main_stack ();
    });
    QObject::connect (peers_refresh, &QPushButton::released, [this] ()
    {
        refresh_peers ();
    });
    QObject::connect (ledger_refresh, &QPushButton::released, [this] ()
    {
        refresh_ledger ();
    });
    QObject::connect (ledger_back, &QPushButton::released, [this] ()
    {
        client.pop_main_stack ();
    });
    QObject::connect (wallet_add_key_button, &QPushButton::released, [this] ()
    {
      QString key_text_wide (wallet_key_line->text ());
      std::string key_text (key_text_wide.toLocal8Bit ());
      rai::private_key key;
      if (!key.decode_hex (key_text))
      {
          QPalette palette;
          palette.setColor (QPalette::Text, Qt::black);
          wallet_key_line->setPalette (palette);
          wallet_key_line->clear ();
          client.wallet->store.insert (key);
          client.refresh_wallet ();
      }
      else
      {
          QPalette palette;
          palette.setColor (QPalette::Text, Qt::red);
          wallet_key_line->setPalette (palette);
      }
    });
    QObject::connect (search_for_receivables, &QPushButton::released, [this] ()
    {
        client.client_m.processor.search_pending ();
    });
    QObject::connect (create_block, &QPushButton::released, [this] ()
    {
        client.push_main_stack (client.block_creation.window);
    });
    QObject::connect (enter_block, &QPushButton::released, [this] ()
    {
        client.push_main_stack (client.block_entry.window);
    });
    refresh_ledger ();
}

void rai_qt::advanced_actions::refresh_peers ()
{
    QStringList peers;
    for (auto i: client.client_m.peers.list ())
    {
        std::stringstream endpoint;
        endpoint << i.endpoint.address ().to_string ();
        endpoint << ':';
        endpoint << i.endpoint.port ();
        endpoint << ' ';
        endpoint << i.last_contact;
        endpoint << ' ';
        endpoint << i.last_attempt;
        QString qendpoint (endpoint.str().c_str ());
        peers << qendpoint;
    }
    peers_model->setStringList (peers);
}

void rai_qt::advanced_actions::refresh_ledger ()
{
    ledger_model->removeRows (0, ledger_model->rowCount ());
    for (auto i (client.client_m.ledger.store.latest_begin()), j (client.client_m.ledger.store.latest_end ()); i != j; ++i)
    {
        QList <QStandardItem *> items;
        std::string account;
        i->first.encode_base58check (account);
        items.push_back (new QStandardItem (QString (account.c_str ())));
        items.push_back (new QStandardItem (QString (std::to_string (rai::scale_down (client.client_m.ledger.balance (i->second.hash))).c_str ())));
        std::string block_hash;
        i->second.hash.encode_hex (block_hash);
        items.push_back (new QStandardItem (QString (block_hash.c_str ())));
        ledger_model->appendRow (items);
    }
}

rai_qt::block_entry::block_entry (rai_qt::client & client_a) :
window (new QWidget),
layout (new QVBoxLayout),
block (new QPlainTextEdit),
status (new QLabel),
process (new QPushButton ("Process")),
back (new QPushButton ("Back")),
client (client_a)
{
    layout->addWidget (block);
    layout->addWidget (status);
    layout->addWidget (process);
    layout->addWidget (back);
    window->setLayout (layout);
    QObject::connect (process, &QPushButton::released, [this] ()
    {
        auto string (block->toPlainText ().toStdString ());
        try
        {
            boost::property_tree::ptree tree;
            std::stringstream istream (string);
            boost::property_tree::read_json (istream, tree);
            auto block_l (rai::deserialize_block_json (tree));
            if (block_l != nullptr)
            {
                client.client_m.processor.process_receive_republish (std::move (block_l));
            }
            else
            {
                status->setStyleSheet ("QLabel { color: red }");
                status->setText ("Unable to parse block");
            }
        }
        catch (std::runtime_error const &)
        {
            status->setStyleSheet ("QLabel { color: red }");
            status->setText ("Unable to parse block");
        }
    });
    QObject::connect (back, &QPushButton::released, [this] ()
    {
        client.pop_main_stack ();
    });
}

rai_qt::block_creation::block_creation (rai_qt::client & client_a) :
window (new QWidget),
layout (new QVBoxLayout),
group (new QButtonGroup),
button_layout (new QHBoxLayout),
send (new QRadioButton ("Send")),
receive (new QRadioButton ("Receive")),
change (new QRadioButton ("Change")),
open (new QRadioButton ("Open")),
account_label (new QLabel ("Account:")),
account (new QLineEdit),
source_label (new QLabel ("Source:")),
source (new QLineEdit),
amount_label (new QLabel ("Amount:")),
amount (new QLineEdit),
destination_label (new QLabel ("Destination:")),
destination (new QLineEdit),
representative_label (new QLabel ("Representative:")),
representative (new QLineEdit),
block (new QPlainTextEdit),
status (new QLabel),
create (new QPushButton ("Create")),
back (new QPushButton ("Back")),
client (client_a)
{
    group->addButton (send);
    group->addButton (receive);
    group->addButton (change);
    group->addButton (open);
    group->setId (send, 0);
    group->setId (receive, 1);
    group->setId (change, 2);
    group->setId (open, 3);
    
    button_layout->addWidget (send);
    button_layout->addWidget (receive);
    button_layout->addWidget (open);
    button_layout->addWidget (change);
    
    layout->addLayout (button_layout);
    layout->addWidget (account_label);
    layout->addWidget (account);
    layout->addWidget (source_label);
    layout->addWidget (source);
    layout->addWidget (amount_label);
    layout->addWidget (amount);
    layout->addWidget (destination_label);
    layout->addWidget (destination);
    layout->addWidget (representative_label);
    layout->addWidget (representative);
    layout->addWidget (block);
    layout->addWidget (status);
    layout->addWidget (create);
    layout->addWidget (back);
    window->setLayout (layout);
    QObject::connect (send, &QRadioButton::toggled, [this] ()
    {
        if (send->isChecked ())
        {
            deactivate_all ();
            activate_send ();
        }
    });
    QObject::connect (receive, &QRadioButton::toggled, [this] ()
    {
        if (receive->isChecked ())
        {
            deactivate_all ();
            activate_receive ();
        }
    });
    QObject::connect (open, &QRadioButton::toggled, [this] ()
    {
        if (open->isChecked ())
        {
            deactivate_all ();
            activate_open ();
        }
    });
    QObject::connect (change, &QRadioButton::toggled, [this] ()
    {
        if (change->isChecked ())
        {
            deactivate_all ();
            activate_change ();
        }
    });
    QObject::connect (create, &QPushButton::released, [this] ()
    {
        switch (group->checkedId ())
        {
            case 0:
                create_send ();
                break;
            case 1:
                create_receive ();
                break;
            case 2:
                create_change ();
                break;
            case 3:
                create_open ();
                break;
            default:
                assert (false);
                break;
        }
    });
    QObject::connect (back, &QPushButton::released, [this] ()
    {
        client.pop_main_stack ();
    });
    send->click ();
}

void rai_qt::block_creation::deactivate_all ()
{
    account_label->hide ();
    account->hide ();
    source_label->hide ();
    source->hide ();
    amount_label->hide ();
    amount->hide ();
    destination_label->hide ();
    destination->hide ();
    representative_label->hide ();
    representative->hide ();
}

void rai_qt::block_creation::activate_send ()
{
    account_label->show ();
    account->show ();
    amount_label->show ();
    amount->show ();
    destination_label->show ();
    destination->show ();
}

void rai_qt::block_creation::activate_receive ()
{
    source_label->show ();
    source->show ();
}

void rai_qt::block_creation::activate_open ()
{
    source_label->show ();
    source->show ();
    representative_label->show ();
    representative->show ();
}

void rai_qt::block_creation::activate_change ()
{
    account_label->show ();
    account->show ();
    representative_label->show ();
    representative->show ();
}

void rai_qt::block_creation::create_send ()
{
    rai::account account_l;
    auto error (account_l.decode_base58check (account->text ().toStdString ()));
    if (!error)
    {
        rai::amount amount_l;
        error = amount_l.decode_hex (amount->text ().toStdString ());
        if (!error)
        {
            rai::account destination_l;
            error = destination_l.decode_base58check (destination->text ().toStdString ());
            if (!error)
            {
                rai::private_key key;
                if (!client.wallet->store.fetch (account_l, key))
                {
                    auto balance (client.client_m.ledger.account_balance (account_l));
                    if (amount_l.number () <= balance)
                    {
                        rai::frontier frontier;
                        auto error (client.client_m.store.latest_get (account_l, frontier));
                        assert (!error);
                        rai::send_block send;
                        send.hashables.destination = destination_l;
                        send.hashables.previous = frontier.hash;
                        send.hashables.balance = rai::amount (balance - amount_l.number ());
                        rai::sign_message (key, account_l, send.hash (), send.signature);
                        key.clear ();
                        send.work = client.client_m.ledger.create_work (send);
                        std::string block_l;
                        send.serialize_json (block_l);
                        block->setPlainText (QString (block_l.c_str ()));
                        status->setStyleSheet ("QLabel { color: black }");
                        status->setText ("Created block");
                    }
                    else
                    {
                        status->setStyleSheet ("QLabel { color: red }");
                        status->setText ("Insufficient balance");
                    }
                }
                else
                {
                    status->setStyleSheet ("QLabel { color: red }");
                    status->setText ("Account is not in wallet");
                }
            }
            else
            {
                status->setStyleSheet ("QLabel { color: red }");
                status->setText ("Unable to decode destination");
            }
        }
        else
        {
            status->setStyleSheet ("QLabel { color: red }");
            status->setText ("Unable to decode amount");
        }
    }
    else
    {
        status->setStyleSheet ("QLabel { color: red }");
        status->setText ("Unable to decode account");
    }
}

void rai_qt::block_creation::create_receive ()
{
    rai::block_hash source_l;
    auto error (source_l.decode_hex (source->text ().toStdString ()));
    if (!error)
    {
        rai::receivable receivable;
        if (!client.client_m.store.pending_get (source_l, receivable))
        {
            rai::frontier frontier;
            auto error (client.client_m.store.latest_get (receivable.destination, frontier));
            if (!error)
            {
                rai::private_key key;
                auto error (client.wallet->store.fetch (receivable.destination, key));
                if (!error)
                {
                    rai::receive_block receive;
                    receive.hashables.previous = frontier.hash;
                    receive.hashables.source = source_l;
                    rai::sign_message (key, receivable.destination, receive.hash (), receive.signature);
                    key.clear ();
                    receive.work = client.client_m.ledger.create_work (receive);
                    std::string block_l;
                    receive.serialize_json (block_l);
                    block->setPlainText (QString (block_l.c_str ()));
                    status->setStyleSheet ("QLabel { color: black }");
                    status->setText ("Created block");
                }
                else
                {
                    status->setStyleSheet ("QLabel { color: red }");
                    status->setText ("Account is not in wallet");
                }
            }
            else
            {
                status->setStyleSheet ("QLabel { color: red }");
                status->setText ("Account not yet open");
            }
        }
        else
        {
            status->setStyleSheet ("QLabel { color: red }");
            status->setText ("Source block is not pending to receive");
        }
    }
    else
    {
        status->setStyleSheet ("QLabel { color: red }");
        status->setText ("Unable to decode source");
    }
}

void rai_qt::block_creation::create_change ()
{
    rai::account account_l;
    auto error (account_l.decode_base58check (account->text ().toStdString ()));
    if (!error)
    {
        rai::account representative_l;
        error = representative_l.decode_base58check (representative->text ().toStdString ());
        if (!error)
        {
            rai::frontier frontier;
            auto error (client.client_m.store.latest_get (account_l, frontier));
            if (!error)
            {
                rai::private_key key;
                auto error (client.wallet->store.fetch (account_l, key));
                if (!error)
                {
                    rai::change_block change (representative_l, frontier.hash, key, account_l);
                    key.clear ();
                    change.work = client.client_m.ledger.create_work (change);
                    std::string block_l;
                    change.serialize_json (block_l);
                    block->setPlainText (QString (block_l.c_str ()));
                    status->setStyleSheet ("QLabel { color: black }");
                    status->setText ("Created block");
                }
                else
                {
                    status->setStyleSheet ("QLabel { color: red }");
                    status->setText ("Account is not in wallet");
                }
            }
            else
            {
                status->setStyleSheet ("QLabel { color: red }");
                status->setText ("Account not yet open");
            }
        }
        else
        {
            status->setStyleSheet ("QLabel { color: red }");
            status->setText ("Unable to decode representative");
        }
    }
    else
    {
        status->setStyleSheet ("QLabel { color: red }");
        status->setText ("Unable to decode account");
    }
}

void rai_qt::block_creation::create_open ()
{
    rai::block_hash source_l;
    auto error (source_l.decode_hex (source->text ().toStdString ()));
    if (!error)
    {
        rai::account representative_l;
        error = representative_l.decode_base58check (representative->text ().toStdString ());
        if (!error)
        {
            rai::receivable receivable;
            if (!client.client_m.store.pending_get (source_l, receivable))
            {
                rai::frontier frontier;
                auto error (client.client_m.store.latest_get (receivable.destination, frontier));
                if (error)
                {
                    rai::private_key key;
                    auto error (client.wallet->store.fetch (receivable.destination, key));
                    if (!error)
                    {
                        rai::open_block open;
                        open.hashables.source = source_l;
                        open.hashables.representative = representative_l;
                        rai::sign_message (key, receivable.destination, open.hash (), open.signature);
                        key.clear ();
                        open.work = client.client_m.ledger.create_work (open);
                        std::string block_l;
                        open.serialize_json (block_l);
                        block->setPlainText (QString (block_l.c_str ()));
                        status->setStyleSheet ("QLabel { color: black }");
                        status->setText ("Created block");
                    }
                    else
                    {
                        status->setStyleSheet ("QLabel { color: red }");
                        status->setText ("Account is not in wallet");
                    }
                }
                else
                {
                    status->setStyleSheet ("QLabel { color: red }");
                    status->setText ("Account already open");
                }
            }
            else
            {
                status->setStyleSheet ("QLabel { color: red }");
                status->setText ("Source block is not pending to receive");
            }
        }
        else
        {
            status->setStyleSheet ("QLabel { color: red }");
            status->setText ("Unable to decode representative");
        }
    }
    else
    {
        status->setStyleSheet ("QLabel { color: red }");
        status->setText ("Unable to decode source");
    }
}