# -*- coding: utf-8 -*-
import logging
import json
import boto3

import ask_sdk_core.utils as ask_utils
from ask_sdk_core.skill_builder import SkillBuilder
from ask_sdk_core.dispatch_components import AbstractRequestHandler, AbstractExceptionHandler
from ask_sdk_core.handler_input import HandlerInput
from ask_sdk_model import Response

# -------------------------------
# CONFIGURACIÓN AWS
# -------------------------------

iot_client = boto3.client('iot-data')
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('UserThingMapping')

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

# -------------------------------
# OBTENER DISPOSITIVO DEL USUARIO
# -------------------------------
def get_thing_name(handler_input):
    try:
        user_id = handler_input.request_envelope.session.user.user_id
        response = table.get_item(Key={'user_id': user_id})
        if "Item" in response:
            return response["Item"]["thing_name"]
    except Exception as e:
        logger.error(f"Error DynamoDB: {e}")
    return None

# -------------------------------
# MAPEOS INTERNOS
# -------------------------------
def map_dispositivo(dispositivo, lugar):
    dispositivo = dispositivo.lower()
    lugar = lugar.lower()
    
    if dispositivo == "luz":
        return f"luz_{lugar}"
    if dispositivo == "puerta":
        return f"puerta_{lugar}"
    if dispositivo == "ventana":
        return f"ventana_{lugar}"
    if dispositivo == "movimiento":
        return f"movimiento_{lugar}"

    return None

def map_accion(accion):
    accion = accion.lower()
    if accion in ["encender"]:
        return "ON"
    if accion in ["apagar"]:
        return "OFF"
    if accion in ["abrir"]:
        return "OPEN"
    if accion in ["cerrar"]:
        return "CLOSED"
    return None


# ======================================================
# INTENT PRINCIPAL: CONTROLAR DISPOSITIVOS
# ======================================================
class ControlarDispositivoIntentHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_intent_name("ControlarDispositivoIntent")(handler_input)

    def handle(self, handler_input):

        slots = handler_input.request_envelope.request.intent.slots

        accion = slots["accion"].value if "accion" in slots else None
        dispositivo = slots["dispositivo"].value if "dispositivo" in slots else None
        lugar = slots["lugar"].value if "lugar" in slots else "sala"

        thing_name = get_thing_name(handler_input)
        if thing_name is None:
            return handler_input.response_builder.speak(
                "No encuentro tu dispositivo asociado en la base de datos."
            ).response

        atributo = map_dispositivo(dispositivo, lugar)
        valor = map_accion(accion)

        if atributo is None or valor is None:
            return handler_input.response_builder.speak(
                "No entendí bien qué dispositivo quieres controlar."
            ).response

        # Construir payload del shadow
        payload = {
            "state": {
                "desired": {
                    atributo: valor
                }
            }
        }

        # Enviar al shadow
        iot_client.update_thing_shadow(
            thingName=thing_name,
            payload=json.dumps(payload).encode("utf-8")
        )

        speak_output = f"{accion} {dispositivo} de la {lugar}."
        return handler_input.response_builder.speak(speak_output).ask("¿Necesitas algo más?").response


# ======================================================
# INTENT: CONSULTAR ESTADO
# ======================================================
class EstadoDispositivoIntentHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_intent_name("EstadoDispositivoIntent")(handler_input)

    def handle(self, handler_input):

        slots = handler_input.request_envelope.request.intent.slots

        dispositivo = slots["dispositivo"].value if "dispositivo" in slots else None
        lugar = slots["lugar"].value if "lugar" in slots else "sala"

        thing_name = get_thing_name(handler_input)

        atributo = map_dispositivo(dispositivo, lugar)

        try:
            response = iot_client.get_thing_shadow(thingName=thing_name)
            shadow = json.loads(response["payload"].read().decode())
            estado = shadow["state"]["reported"].get(atributo, "desconocido")

            speak_output = f"El estado de la {dispositivo} de la {lugar} es {estado}."

        except Exception as e:
            logger.error(f"Error shadow: {e}")
            speak_output = "No pude obtener el estado del dispositivo."

        return handler_input.response_builder.speak(speak_output).ask("¿Necesitas algo más?").response


# ======================================================
# HANDLERS BASE
# ======================================================
class LaunchRequestHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_request_type("LaunchRequest")(handler_input)

    def handle(self, handler_input):
        return handler_input.response_builder.speak(
            "Bienvenido al sistema inteligente. ¿Qué deseas hacer?"
        ).ask(
            "Puedes decir: enciende la luz de la sala."
        ).response


class HelpIntentHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_intent_name("AMAZON.HelpIntent")(handler_input)

    def handle(self, handler_input):
        return handler_input.response_builder.speak(
            "Puedes dar órdenes como: abre la puerta de la cocina."
        ).ask("¿Qué deseas hacer?").response


class CancelOrStopIntentHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_intent_name("AMAZON.CancelIntent")(handler_input) or \
               ask_utils.is_intent_name("AMAZON.StopIntent")(handler_input)

    def handle(self, handler_input):
        return handler_input.response_builder.speak("Adiós.").response


class FallbackIntentHandler(AbstractRequestHandler):
    def can_handle(self, handler_input):
        return ask_utils.is_intent_name("AMAZON.FallbackIntent")(handler_input)

    def handle(self, handler_input):
        return handler_input.response_builder.speak(
            "No entendí tu orden. Intenta con: enciende la luz de la sala."
        ).ask(
            "¿Qué deseas hacer?"
        ).response


class CatchAllExceptionHandler(AbstractExceptionHandler):
    def can_handle(self, handler_input, exception):
        return True

    def handle(self, handler_input, exception):
        logger.error(exception, exc_info=True)
        return handler_input.response_builder.speak(
            "Ocurrió un error procesando tu solicitud."
        ).ask("¿Deseas intentar otra cosa?").response


# ======================================================
# CONSTRUIR SKILL
# ======================================================
sb = SkillBuilder()

sb.add_request_handler(LaunchRequestHandler())
sb.add_request_handler(ControlarDispositivoIntentHandler())
sb.add_request_handler(EstadoDispositivoIntentHandler())
sb.add_request_handler(HelpIntentHandler())
sb.add_request_handler(CancelOrStopIntentHandler())
sb.add_request_handler(FallbackIntentHandler())
sb.add_exception_handler(CatchAllExceptionHandler())

lambda_handler = sb.lambda_handler()
