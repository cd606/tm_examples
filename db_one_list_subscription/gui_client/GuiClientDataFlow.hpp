#ifndef GUI_CLIENT_DATA_FLOW_HPP_
#define GUI_CLIENT_DATA_FLOW_HPP_

#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockImporter.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>
#include <tm_kit/basic/transaction/named_value_store/DataModel.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/CrossGuidComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include "db_one_list_subscription/DBData.hpp"

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

using DI = basic::transaction::named_value_store::DI<db_data>;
using GS = basic::transaction::named_value_store::GS<transport::CrossGuidComponent::IDType,db_data>;
using TI = basic::transaction::named_value_store::TI<db_data>;

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::CrossGuidComponent,
    transport::rabbitmq::RabbitMQComponent,
    transport::ClientSideSimpleIdentityAttacherComponent<
        std::string
        , TI::Transaction>,
    transport::ClientSideSimpleIdentityAttacherComponent<
        std::string
        , GS::Input>
>;
using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;

TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT(GuiExitEvent);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT_SERIALIZE(GuiExitEvent);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT(UnsubscribeConfirmed);
TM_BASIC_CBOR_CAPABLE_EMPTY_STRUCT_SERIALIZE(UnsubscribeConfirmed);

extern void guiClientDataFlow(
    R &r
    , std::string const &clientName
    , R::Sourceoid<TI::Transaction> transactionCommandSource
    , R::Sourceoid<GuiExitEvent> exitEventSource
    , R::Sinkoid<DI::FullUpdate> updateSink
    , R::Sinkoid<UnsubscribeConfirmed> unsubscribeSink
);

#endif